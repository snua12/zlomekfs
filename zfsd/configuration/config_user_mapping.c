#include "system.h"
#include "config_user_mapping.h"
#include "config_parser.h"
#include "user-group.h"
#include "dir.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"
#include "varray.h"
#include "config_iface.h"

/* ! Read list of user mapping.  If SID == 0 read the default user mapping
   from CONFIG_DIR/user/default else read the special mapping for node SID.  */

bool read_user_mapping(zfs_fh * user_dir, uint32_t sid)
{
	dir_op_res user_mapping_res;
	int32_t r;
	string node_name_;

	if (sid == 0)
	{
		node_name_.str = "default";
		node_name_.len = strlen("default");
	}
	else
	{
		node nod;

		nod = node_lookup(sid);
		if (!nod)
			return false;

		xstringdup(&node_name_, &nod->name);
		zfsd_mutex_unlock(&nod->mutex);
	}

	r = zfs_extended_lookup(&user_mapping_res, user_dir, node_name_.str);
	if (r != ZFS_OK)
	{
		if (sid != 0)
			free(node_name_.str);
		return true;
	}

	zfs_file * file = zfs_fopen(&user_mapping_res.file);
	if (file == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read shared group mapping.\n");
		return false;
	}

	config_t config;
	config_init(&config);
	int rv;
	rv =  config_read(&config, zfs_fdget(file));
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared user mapping.\n");
		zfs_fclose(file);
		return false;
	}

	varray users_mappings;
	varray_create(&users_mappings, sizeof(user_mapping),4);

	rv = read_user_mapping_shared_config(&config, node_name_.str, &users_mappings);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared user mapping.\n");
	}

	update_user_mappings(&users_mappings, sid);

	varray_destroy(&users_mappings);

	config_destroy(&config);
	zfs_fclose(file);

	if (sid != 0)
		free(node_name_.str);


	return (rv == CONFIG_TRUE);
}
