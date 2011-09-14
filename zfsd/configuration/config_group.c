#include "system.h"
#include "log.h"
#include "user-group.h"
#include "config_parser.h"
#include "config_group.h"
#include "dir.h"
#include "fh.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"

/* ! Read list of groups from CONFIG_DIR/group_list.  */

bool read_group_list(zfs_fh * config_dir)
{
	dir_op_res group_list_res;
	int32_t r;

	r = zfs_extended_lookup(&group_list_res, config_dir, "group_list");
	if (r != ZFS_OK)
		return false;

	zfs_file * file = zfs_fopen(&group_list_res.file);
	if (file == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read shared group list.\n");
		return false;
	}

	config_t config;
	config_init(&config);
	int rv;
	rv =  config_read(&config, zfs_fdget(file));
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared group list.\n");
		zfs_fclose(file);
		return false;
	}

	rv = read_group_list_shared_config(&config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared group list.\n");
	}

	config_destroy(&config);
	zfs_fclose(file);

	return (rv == CONFIG_TRUE);
}
