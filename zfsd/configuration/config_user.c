#include "system.h"
#include "config_user.h"
#include "fh.h"
#include "config_parser.h"
#include "user-group.h"
#include "dir.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"


/* ! Read list of users from CONFIG_DIR/user_list.  */

bool read_user_list(zfs_fh * config_dir)
{
	dir_op_res user_list_res;
	int32_t r;

	r = zfs_extended_lookup(&user_list_res, config_dir, "user_list");
	if (r != ZFS_OK)
		return false;

	zfs_file * file = zfs_fopen(&user_list_res.file);
	if (file == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read shared user list.\n");
		return false;
	}

	config_t config;
	config_init(&config);
	int rv;
	rv =  config_read(&config, zfs_fdget(file));
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared user list.\n");
		zfs_fclose(file);
		return false;
	}

	rv = read_user_list_shared_config(&config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared user list.\n");
	}

	config_destroy(&config);
	zfs_fclose(file);

	return (rv == CONFIG_TRUE);
}
