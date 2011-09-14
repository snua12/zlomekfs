#include "config_volume.h"
#include "configuration.h"
#include "config_parser.h"
#include "dir.h"
#include "zfsd.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"
#include "pthread-wrapper.h"

/* ! Initialize config volume so that we could read configuration.  */
bool init_config_volume(void)
{
	volume vol;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	vol = volume_lookup_nolock(VOLUME_ID_CONFIG);
	if (!vol)
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"Config volume (ID == %" PRIu32 ") does not exist.\n",
				VOLUME_ID_CONFIG);
		goto out;
	}

	// config node was set by command line option node=1:node_a:HOST_NAME_OF_NODE_A
	// zfs_config.config_node = xstrdup(zopts.node);
	if (zfs_config.config_node)
	{
		string parts[3];
		uint32_t sid;
		node nod;
		string path;

		if (split_and_trim(zfs_config.config_node, 3, parts) == 3)
		{
			if (sscanf(parts[0].str, "%" PRIu32, &sid) != 1)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"Wrong format of node option\n");
				goto out_usage;
			}
			else if (sid == 0 || sid == (uint32_t) - 1)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"Node ID must not be 0 or %" PRIu32 "\n",
						(uint32_t) - 1);
				goto out_usage;
			}
			else if (sid == zfs_config.this_node.node_id)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"The ID of the config node must be "
						"different from the ID of the local node\n");
				goto out_usage;
			}
			else if (parts[1].len == 0)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"Node name must not be empty\n");
				goto out_usage;
			}
			else if (parts[1].len == zfs_config.this_node.node_name.len
					 && strcmp(parts[1].str, zfs_config.this_node.node_name.str) == 0)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"The name of the config node must be "
						"different from the name of the local node\n");
				goto out_usage;
			}
			else if (parts[2].len == 0)
			{
				message(LOG_ERROR, FACILITY_CONFIG,
						"Node host name must not be empty\n");
				goto out_usage;
			}
			else
			{
				/* Create the node and set it as master of config volume.  */
				zfsd_mutex_lock(&node_mutex);
				nod = node_create(sid, &parts[1], &parts[2]);
				zfsd_mutex_unlock(&nod->mutex);
				zfsd_mutex_unlock(&node_mutex);

				volume_set_common_info_wrapper(vol, "config", "/config", nod);
				xstringdup(&path, &vol->local_path);
				zfsd_mutex_unlock(&vol->mutex);
				zfsd_mutex_unlock(&volume_mutex);
				zfsd_mutex_unlock(&fh_mutex);

				/* Recreate the directory where config volume is cached.  */
				recursive_unlink(&path, VOLUME_ID_VIRTUAL, false, false,
								 false);
				zfsd_mutex_lock(&fh_mutex);
				vol = volume_lookup(VOLUME_ID_CONFIG);
#ifdef ENABLE_CHECKING
				if (!vol)
					abort();
#endif
				if (volume_set_local_info(&vol, &path, vol->size_limit))
				{
					if (vol)
						zfsd_mutex_unlock(&vol->mutex);
				}
				else
				{
					zfsd_mutex_unlock(&vol->mutex);
					message(LOG_CRIT, FACILITY_CONFIG,
							"Could not initialize config volume.\n");
					goto out_fh;
				}
				zfsd_mutex_unlock(&fh_mutex);

				free(zfs_config.config_node);
				zfs_config.config_node = NULL;
			}
		}
		else
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"Wrong format of node option\n");
			goto out_usage;
		}
	}
	else
	{
		volume_set_common_info_wrapper(vol, "config", "/config", this_node);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&volume_mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}
	return true;

  out_usage:
	zfsd_mutex_unlock(&vol->mutex);
	usage();

  out:
	zfsd_mutex_unlock(&volume_mutex);

  out_fh:
	zfsd_mutex_unlock(&fh_mutex);
	destroy_all_volumes();
	return false;
}


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
