#include "config_volume.h"
#include "configuration.h"
#include "config_iface.h"
#include "dir.h"
#include "zfsd.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"
#include "pthread-wrapper.h"

/* ! \brief Data for process_line_volume_hierarchy.  */
typedef struct volume_hierarchy_data_def
{
	varray hierarchy;
	uint32_t vid;
	uint32_t depth;
	string *name;
	string *mountpoint;
	char *master_name;
} volume_hierarchy_data;

/// add slaves nodes to volume
static bool_t volume_set_slave_info(volume vol, varray * slave_names)
{
	unsigned int i;

	htab_empty(vol->slaves);
	for (i = 0; i < VARRAY_USED(*slave_names); ++i)
	{
		char * slave_name = VARRAY_ACCESS(*slave_names, i, char *);

		string str;
		str.str =  slave_name;
		str.len = strlen(slave_name);
		node nod = node_lookup_name(&str);
		if (nod == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Slave node \"%s\" was not found.\n", slave_name);
			continue;
		}

		/* Insert slave node into slave hash table. */
		void ** slot = htab_find_slot_with_hash(vol->slaves, nod, node_hash_name(nod),
										INSERT);
		*slot = nod;
		zfsd_mutex_unlock(&nod->mutex);
	}

	return true;
}

/// updates volume by array of volume_entry
static bool_t update_volumes(varray * volumes)
{
	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);

	unsigned int i;
	for (i = 0; i < VARRAY_USED(*volumes); ++i)
	{
		//FIXME: copy of structure volume_entry_ve
		volume_entry ve = VARRAY_ACCESS(*volumes, i, volume_entry);
		volume vol = volume_lookup_nolock(ve.id);
		if (vol == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Volume with id %d not found, please add it to local config.\n", ve.id);
			continue;
		}

		node master_node;
		if (stringlen(&ve.master_name))
		{
			master_node = node_lookup_name(&ve.master_name);
			if (master_node == NULL)
			{
				message(LOG_ERROR, FACILITY_CONFIG, "Master node name \"%s\" is invaled\n", ve.master_name.str);
				zfsd_mutex_unlock(&vol->mutex);
				continue;
			}
		}
		else
		{
			master_node = this_node;
			zfsd_mutex_lock(&this_node->mutex);
		}

		volume_set_common_info(vol, &ve.name, &ve.mountpoint, master_node);
		zfsd_mutex_unlock(&master_node->mutex);

		if (vol->slaves != NULL)
			volume_set_slave_info(vol, &ve.slave_names);

		zfsd_mutex_unlock(&vol->mutex);

	}

	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);

	return true;

}

/* ! Read list of volumes from CONFIG_DIR/volume_list.  */

bool read_volume_list(zfs_fh * config_dir)
{
	dir_op_res volume_list_res;
	int32_t r;

	r = zfs_extended_lookup(&volume_list_res, config_dir, "volume_list");
	if (r != ZFS_OK)
		return false;

	zfs_file * file = zfs_fopen(&volume_list_res.file);
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

	varray volumes;
	varray_create(&volumes, sizeof(volume_entry), 4);

	rv = read_volume_list_shared_config(&config, &volumes);
	if (rv == CONFIG_TRUE)
	{
		rv = update_volumes(&volumes);
		
	}
	else
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared user list.\n");
	}

	config_destroy(&config);
	zfs_fclose(file);
	varray_destroy(&volumes);

	return (rv == CONFIG_TRUE);
}
