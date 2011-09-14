#include "system.h"
#include "log.h"
#include "config_parser.h"
#include "user-group.h"
#include "dir.h"
#include "config_group_mapping.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"
#include "varray.h"

static bool_t update_group_mappings(varray * groups_mappings, uint32_t sid)
{
	node nod = NULL;
	if (sid > 0)
	{
		nod = node_lookup(sid);
		if (nod == NULL)
			return false;
	}

	zfsd_mutex_lock(&users_groups_mutex);

	unsigned int i;
	for (i = 0; i < VARRAY_USED(*groups_mappings); ++i)
	{
		group_mapping gm = VARRAY_ACCESS(*groups_mappings, i, group_mapping);
		group_mapping_create(&(gm.zfs_group), &(gm.node_group), nod);
	}

	zfsd_mutex_unlock(&users_groups_mutex);

	if (sid > 0)
	{
		zfsd_mutex_unlock(&nod->mutex);
	}

	return true;

}



/* ! Read list of group mapping.  If SID == 0 read the default group mapping
   from CONFIG_DIR/group/default else read the special mapping for node SID.  */

bool read_group_mapping(zfs_fh * group_dir, uint32_t sid)
{
	dir_op_res group_mapping_res;
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

	r = zfs_extended_lookup(&group_mapping_res, group_dir, node_name_.str);
	if (r != ZFS_OK)
	{
		if (sid != 0)
			free(node_name_.str);
		return true;
	}

	zfs_file * file = zfs_fopen(&group_mapping_res.file);
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
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared group mapping.\n");
		zfs_fclose(file);
		return false;
	}

	varray groups_mappings;
	varray_create(&groups_mappings, sizeof(group_mapping), 4);

	rv = read_group_mapping_shared_config(&config, node_name_.str, &groups_mappings);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared group mapping.\n");
	}

	update_group_mappings(&groups_mappings, sid);

	varray_destroy(&groups_mappings);

	config_destroy(&config);
	zfs_fclose(file);

	if (sid != 0)
		free(node_name_.str);

	return (rv == CONFIG_TRUE);
}
