#include "config_volume.h"
#include "configuration.h"
#include "config_parser.h"
#include "dir.h"
#include "zfsd.h"

/* ! Saved information about config volume because we need to update it after
   information about every volume was read.  */

static uint32_t saved_vid;
static string saved_name;
static string saved_mountpoint;

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
			else if (sid == this_node_id)
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
			else if (parts[1].len == node_name.len
					 && strcmp(parts[1].str, node_name.str) == 0)
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


/* ! Process line LINE number LINE_NUM of volume hierarchy file FILE_NAME and
   update hierarchy DATA.  */

static int process_line_volume_hierarchy(char *line,
							  ATTRIBUTE_UNUSED const char *file_name,
							  ATTRIBUTE_UNUSED unsigned int line_num,
							  void *data)
{
	volume_hierarchy_data *d = (volume_hierarchy_data *) data;
	char *name;
	uint32_t i;
	volume vol;
	node nod;
	string str;
	void **slot;

	/* count node deep */
	for (i = 0; line[i] == ' '; i++)
		;
	if (line[i] == 0)
		return 0;

	if (d->depth == 0)
	{
		/* Free superfluous records.  */
		while (VARRAY_USED(d->hierarchy) > i)
		{
			name = VARRAY_TOP(d->hierarchy, char *);
			if (name)
				free(name);
			VARRAY_POP(d->hierarchy);
		}

		/* TODO: is this correct eg spaced after node_name or
		   node_name_with_some_suffixes */
		if (strncmp(line + i, node_name.str, node_name.len + 1) == 0)
		{
			char *master_name = NULL;

			/* We are processing the local node.  */

			d->depth = i + 1;
			while (VARRAY_USED(d->hierarchy) > 0)
			{
				master_name = VARRAY_TOP(d->hierarchy, char *);
				if (master_name)
					break;
				VARRAY_POP(d->hierarchy);
			}


			// TODO: if master_name is not set then use this_node, everything
			// without warning !!!!
			if (master_name)
			{
				str.str = master_name;
				str.len = strlen(master_name);
				nod = node_lookup_name(&str);
				if (nod)
					zfsd_mutex_unlock(&nod->mutex);
			}
			else
				nod = this_node;

			if (nod)
			{
				zfsd_mutex_lock(&fh_mutex);
				zfsd_mutex_lock(&volume_mutex);
				vol = volume_lookup_nolock(d->vid);
				if (!vol)
					vol = volume_create(d->vid);
				else
				{
					if (vol->slaves)
						htab_empty(vol->slaves);
				}

				/* Do not set the common info of the config volume because the 
				   file is still open and changing the volume master from
				   this_node to another one would cause zfs_close think that
				   it has to save the interval files.  */
				if (d->vid != VOLUME_ID_CONFIG)
					volume_set_common_info(vol, d->name, d->mountpoint, nod);
				else
				{
					if (master_name)
						d->master_name = xstrdup(master_name);
					else
						d->master_name = NULL;
				}

				zfsd_mutex_unlock(&vol->mutex);
				zfsd_mutex_unlock(&volume_mutex);
				zfsd_mutex_unlock(&fh_mutex);

				/* Continue reading the file because we need to read the list
				   of nodes whose master is local node.  */
				if (vol->slaves && !vol->marked)
					return 0;
			}

			return 1;
		}

		/* Add missing empty records.  */
		while (VARRAY_USED(d->hierarchy) < i)
			VARRAY_PUSH(d->hierarchy, NULL, char *);

		/* Add node name.  */
		name = xstrdup(line + i);
		VARRAY_PUSH(d->hierarchy, name, char *);
	}
	else
	{
		/* We have created/updated the volume, read the list of nodes whose
		   master is local node.  */

		if (i < d->depth)
		{
			/* The subtree of local node has been processed, stop reading the
			   file.  */
			return 1;
		}

		/* Free superfluous records.  */
		while (VARRAY_USED(d->hierarchy) > i)
		{
			name = VARRAY_TOP(d->hierarchy, char *);
			if (name)
				free(name);
			VARRAY_POP(d->hierarchy);
		}

		/* Push missing empty records.  */
		while (VARRAY_USED(d->hierarchy) < i)
			VARRAY_PUSH(d->hierarchy, NULL, char *);

		/* Do not add local node to list of slaves.  */
		if (strcmp(line + i, this_node->name.str) == 0)
			return 0;

		name = xstrdup(line + i);
		VARRAY_PUSH(d->hierarchy, name, char *);

		/* All records in hierarchy upto current node must be NULL so that
		   local node would be master of current node.  */
		for (i = d->depth; i < VARRAY_USED(d->hierarchy) - 2; i++)
			if (VARRAY_ACCESS(d->hierarchy, i, char *) != NULL)
			{
				/* The current node is not direct descendant of local node so
				   continue reading the file.  */
				return 0;
			}

		vol = volume_lookup(d->vid);
		if (!vol)
		{
			/* Volume was destroyed meanwhile.  */
			return 1;
		}

#ifdef ENABLE_CHECKING
		if (vol->slaves == NULL)
			abort();
#endif

		str.str = line + i;
		str.len = strlen(line + i);
		nod = node_lookup_name(&str);

		/* The current node was't found so continue reading the file.  */
		if (nod == NULL)
		{
			zfsd_mutex_unlock(&vol->mutex);
			return 0;
		}

		if (vol->master == nod)
		{
			zfsd_mutex_unlock(&nod->mutex);
			zfsd_mutex_unlock(&vol->mutex);
			return 0;
		}

		/* Insert slave node into slave hash table. */
		slot = htab_find_slot_with_hash(vol->slaves, nod, node_hash_name(nod),
										INSERT);
		*slot = nod;
		zfsd_mutex_unlock(&nod->mutex);
		zfsd_mutex_unlock(&vol->mutex);
	}

	return 0;
}

/* ! Read appropriate file in VOLUME_INFO_DIR and process info about volume
   VID with name NAME and volume mountpoint MOUNTPOINT.  */

void
read_volume_hierarchy(zfs_fh * volume_hierarchy_dir, uint32_t vid,
					  string * name, string * mountpoint)
{
	volume_hierarchy_data data;
	dir_op_res file_res;
	char *file_name, *master_name;
	string str;
	volume vol;
	node nod;
	int32_t r;

	r = zfs_extended_lookup(&file_res, volume_hierarchy_dir, name->str);
	if (r != ZFS_OK)
		return;

	varray_create(&data.hierarchy, sizeof(char *), 4);
	data.vid = vid;
	data.depth = 0;
	data.name = name;
	data.mountpoint = mountpoint;

	file_name = xstrconcat(2, "config:/volume/", name->str);
	process_file_by_lines(&file_res.file, file_name,
						  process_line_volume_hierarchy, &data);
	free(file_name);

	/* Setting the common info of config volume was postponed so set it now.  */
	if (vid == VOLUME_ID_CONFIG)
	{
		if (data.master_name)
		{
			str.str = data.master_name;
			str.len = strlen(data.master_name);
			nod = node_lookup_name(&str);
			if (nod)
				zfsd_mutex_unlock(&nod->mutex);
			free(data.master_name);
		}
		else
			nod = this_node;

		zfsd_mutex_lock(&fh_mutex);
		zfsd_mutex_lock(&volume_mutex);
		vol = volume_lookup_nolock(vid);
#ifdef ENABLE_CHECKING
		if (!vol)
			abort();
#endif
		volume_set_common_info(vol, name, mountpoint, nod);

		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&volume_mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}

	/* Set the common volume info for nodes which were not listed in volume
	   hierarchy.  */
	if (VARRAY_USED(data.hierarchy) > 0)
	{
		unsigned int i;
		string str2;
		volume vol2;
		node nod2;

		master_name = NULL;
		for (i = 0; i < VARRAY_USED(data.hierarchy); i++)
		{
			master_name = VARRAY_ACCESS(data.hierarchy, i, char *);
			if (master_name)
				break;
		}

		if (master_name)
		{
			str2.str = master_name;
			str2.len = strlen(master_name);
			nod2 = node_lookup_name(&str2);
			if (!nod2)
				goto out;
			zfsd_mutex_unlock(&nod2->mutex);

			zfsd_mutex_lock(&fh_mutex);
			zfsd_mutex_lock(&volume_mutex);
			vol2 = volume_lookup_nolock(vid);
			if (!vol2)
				vol2 = volume_create(vid);
			else
			{
				if (!vol2->marked)
				{
					zfsd_mutex_unlock(&vol2->mutex);
					zfsd_mutex_unlock(&volume_mutex);
					zfsd_mutex_unlock(&fh_mutex);
					goto out;
				}

				if (vol2->slaves)
					htab_empty(vol2->slaves);
			}
			volume_set_common_info(vol2, name, mountpoint, nod2);
			zfsd_mutex_unlock(&vol2->mutex);
			zfsd_mutex_unlock(&volume_mutex);
			zfsd_mutex_unlock(&fh_mutex);
		}
	}

  out:
	while (VARRAY_USED(data.hierarchy) > 0)
	{
		master_name = VARRAY_TOP(data.hierarchy, char *);
		if (master_name)
			free(master_name);
		VARRAY_POP(data.hierarchy);
	}
	varray_destroy(&data.hierarchy);
}

static bool is_valid_volume_id(uint32_t vid)
{
	return (vid != 0) && (vid != (uint32_t) -1);
}

static bool is_valid_volume_name(string * name)
{
	return (name->len > 0);
}

static bool is_valid_local_path(string * path)
{
#ifndef	ENABLE_LOCAL_PATH
	return (path->str[0] == '/') && (path->len > 0);
#else
	return (path->len > 0);
#endif

}


/* ! Process line LINE number LINE_NUM from file FILE_NAME. Return 0 if we
   should continue reading lines from file.  */

static int
process_line_volume(char *line, const char *file_name, unsigned int line_num,
					void *data)
{
	zfs_fh *volume_hierarchy_dir = (zfs_fh *) data;
	string parts[3];
	uint32_t vid;

	if (split_and_trim(line, 3, parts) == 3)
	{
		if (sscanf(parts[0].str, "%" PRIu32, &vid) != 1)
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"%s:%u: Wrong format of line\n", file_name, line_num);
		}
		else if (!is_valid_volume_id(vid))
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"%s:%u: Volume ID must not be 0 or %" PRIu32 "\n",
					file_name, line_num, (uint32_t) - 1);
		}
		else if (!is_valid_volume_name(parts + 1))
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"%s:%u: Volume name must not be empty\n", file_name,
					line_num);
		}
		else if (!is_valid_local_path(parts + 2))
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"%s:%d: Volume mountpoint must be an absolute path\n",
					file_name, line_num);
		}
		// volume id is readed by first time
		else if (vid == VOLUME_ID_CONFIG && saved_vid == 0)
		{
			volume vol;

			saved_vid = vid;
			xstringdup(&saved_name, &parts[1]);
			xstringdup(&saved_mountpoint, &parts[2]);

			zfsd_mutex_lock(&fh_mutex);
			zfsd_mutex_lock(&volume_mutex);
			vol = volume_lookup_nolock(vid);
#ifdef ENABLE_CHECKING
			if (!vol)
				abort();
			else
#endif
			{
				if (vol->slaves)
					htab_empty(vol->slaves);
			}

			volume_set_common_info(vol, &parts[1], &parts[2], vol->master);
			zfsd_mutex_unlock(&vol->mutex);
			zfsd_mutex_unlock(&volume_mutex);
			zfsd_mutex_unlock(&fh_mutex);
		}
		else
		{
			read_volume_hierarchy(volume_hierarchy_dir, vid, &parts[1],
								  &parts[2]);
		}
	}
	else
	{
		message(LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
				file_name, line_num);
	}

	return 0;
}

/* ! Read list of volumes from CONFIG_DIR/volume_list.  */

bool read_volume_list(zfs_fh * config_dir)
{
	dir_op_res volume_list_res;
	dir_op_res volume_hierarchy_res;
	volume vol;
	int32_t r;

	r = zfs_extended_lookup(&volume_list_res, config_dir, "volume_list");
	if (r != ZFS_OK)
		return false;

	r = zfs_extended_lookup(&volume_hierarchy_res, config_dir, "volume");
	if (r != ZFS_OK)
		return false;

	saved_vid = 0;
	if (!process_file_by_lines(&volume_list_res.file, "config:/volume_list",
							   process_line_volume,
							   &volume_hierarchy_res.file))
		return false;

	if (saved_vid == VOLUME_ID_CONFIG)
	{
		read_volume_hierarchy(&volume_hierarchy_res.file, saved_vid,
							  &saved_name, &saved_mountpoint);
		free(saved_name.str);
		free(saved_mountpoint.str);

		vol = volume_lookup(saved_vid);
		if (!vol)
			goto no_config;
		if (vol->marked)
		{
			zfsd_mutex_unlock(&vol->mutex);
			goto no_config;
		}
		zfsd_mutex_unlock(&vol->mutex);
	}
	else
	{
	  no_config:
		message(LOG_CRIT, FACILITY_CONFIG,
				"config:/volume_list: Config volume does not exist\n");
		return false;
	}

	return true;
}
