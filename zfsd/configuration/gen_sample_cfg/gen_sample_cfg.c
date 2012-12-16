/**
 *  \file gen_sample_cfg.c
 * 
 *  \brief geterates sample local config form zlomekFS
 *  \author Ales Snuparek
 */

/* Copyright (C) 2012 Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libconfig.h>

#ifndef message
#define message(level, facility, ...) fprintf (stderr, __VA_ARGS__)
#endif

static int local_config_set_local_node(config_setting_t * cfg_root, const char * node_name, int node_id, int node_port)
{
	int rv;

	config_setting_t * local_node = config_setting_add(cfg_root, "local_node", CONFIG_TYPE_GROUP);
	if (local_node == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_add() local_name failed\n");
		return CONFIG_FALSE;
	}


	config_setting_t * name = config_setting_add(local_node, "name", CONFIG_TYPE_STRING);
	if (name == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_add() name failed\n");
		return CONFIG_FALSE;
	}

	rv = config_setting_set_string(name, node_name);
	if (rv == CONFIG_FALSE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_set_string() failed\n");
		return CONFIG_FALSE;
	}


	config_setting_t * id = config_setting_add(local_node, "id", CONFIG_TYPE_INT);
	if (id == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_add() id failed\n");
		return CONFIG_FALSE;
	}

	rv = config_setting_set_int(id, node_id);
	if (rv == CONFIG_FALSE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_set_int() id failed\n");
		return CONFIG_FALSE;
	}

	config_setting_t * port = config_setting_add(local_node, "port", CONFIG_TYPE_INT);
	if (port == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_add() port failed\n");
		return CONFIG_FALSE;
	}

	rv = config_setting_set_int(port, node_port);
	if (rv == CONFIG_FALSE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_setting_set_int() port failed\n");
		return CONFIG_FALSE;
	}

	return CONFIG_TRUE;
}

static int local_config_set_volume(config_setting_t * volumes, int volume_id, const char * volume_local_path, int volume_cache_size)
{
	int rv;

	config_setting_t * vol = config_setting_add(volumes, "", CONFIG_TYPE_GROUP);
	if (vol == NULL)
	{
		return CONFIG_FALSE;
	}

	config_setting_t * id = config_setting_add(vol, "id", CONFIG_TYPE_INT);
	if (id == NULL)
	{
		return CONFIG_FALSE;
	}

	rv = config_setting_set_int(id, volume_id);
	if (rv == CONFIG_FALSE)
	{
		return CONFIG_FALSE;
	}

	config_setting_t * local_path = config_setting_add(vol, "local_path", CONFIG_TYPE_STRING);
	if (local_path == NULL)
	{
		return CONFIG_FALSE;
	}

	rv = config_setting_set_string(local_path, volume_local_path);
	if (rv == CONFIG_FALSE)
	{
		return CONFIG_FALSE;
	}

	config_setting_t * cache_size = config_setting_add(vol, "cache_size", CONFIG_TYPE_INT);
	if (cache_size == NULL)
	{
		return CONFIG_FALSE;
	}

	return config_setting_set_int(cache_size, volume_cache_size);
}

static int local_config_set_volumes_list(config_setting_t * cfg_root, const char * prefix)
{
	int rv;
	config_setting_t * volumes = config_setting_add(cfg_root, "volumes", CONFIG_TYPE_LIST);
	if (volumes == NULL)
	{
		return CONFIG_FALSE;
	}

	char volume_path[1024];

	snprintf(volume_path, sizeof(volume_path), "%s%s", prefix, "/var/zfs/config");
	rv = local_config_set_volume(volumes, 1, volume_path, 0);

	snprintf(volume_path, sizeof(volume_path), "%s%s", prefix, "/var/zfs/data");
	rv = local_config_set_volume(volumes, 2, volume_path, 0);
	return CONFIG_TRUE;
}

static int local_config_set_system(config_setting_t * cfg_root, int zfs_mlock, int zfs_metadata_depth)
{
	int rv;
	config_setting_t * cfg_system = config_setting_add(cfg_root, "system", CONFIG_TYPE_GROUP);
	if (system == NULL)
	{
		return CONFIG_FALSE;
	}

	config_setting_t * mlock = config_setting_add(cfg_system, "mlock", CONFIG_TYPE_BOOL);
	if (mlock == NULL)
	{
		return CONFIG_FALSE;
	}

	rv = config_setting_set_bool(mlock, zfs_mlock);
	if (rv == CONFIG_FALSE)
	{
		return CONFIG_FALSE;
	}

	config_setting_t * metadata_tree_depth = config_setting_add(cfg_system, "metadata_tree_depth", CONFIG_TYPE_INT);
	if (metadata_tree_depth == NULL)
	{
		return CONFIG_FALSE;
	}

	return config_setting_set_int(metadata_tree_depth, zfs_metadata_depth);
}

static int local_config_set_versioning(config_setting_t * cfg_root, int enable)
{

	config_setting_t * cfg_versioning = config_setting_add(cfg_root, "versioning", CONFIG_TYPE_GROUP);
	if (cfg_versioning == NULL) return CONFIG_FALSE;

	config_setting_t * cfg_enable = config_setting_add(cfg_versioning, "enable", CONFIG_TYPE_BOOL);
	if (cfg_enable == NULL) return CONFIG_FALSE;
	config_setting_set_bool(cfg_enable, enable);

#if  0
versioning:
{
	# enable versioning
	enable = false;
	# display version files in directory listing
	display = true;
	# age version retention period
	retention_age:
	{
		# -1 is default value
		min = -1;
		max = -1;
	};
	# number of versions to keep with retention.
	retention_num:
	{
		min = -1;
		max = -1;
	};
};

#endif
	return CONFIG_TRUE;
}

static int local_config_set_threads(config_setting_t * cfg_root)
{
	config_setting_t * cfg_threads = config_setting_add(cfg_root, "threads", CONFIG_TYPE_GROUP);
	if (cfg_threads == NULL) return CONFIG_FALSE;
#if 0
threads:
{
#	kernel_thread:
#	{
#		min_spare = 10;
#		max_spare = 10;
#		max_total = 10;
#	};
#	network_thread:
#	{
#		min_spare = 10;
#		max_spare = 10;
#		max_total = 10;
#	};
#	update_thread:
#	{
#		min_spare = 10;
#		max_spare = 10;
#		max_total = 10;
#	};
};

#endif
	return CONFIG_TRUE;
}


static int local_config_set_users(config_setting_t * cfg_root, int id)
{
	config_setting_t * cfg_users = config_setting_add(cfg_root, "users", CONFIG_TYPE_GROUP);
	if (cfg_users == NULL) return CONFIG_FALSE;

	config_setting_t * cfg_default_uid = config_setting_add(cfg_users, "default_uid", CONFIG_TYPE_INT);
	if (cfg_default_uid == NULL) return CONFIG_FALSE;
	return config_setting_set_int(cfg_default_uid, id);
}

static int local_config_set_groups(config_setting_t * cfg_root, int id)
{
	config_setting_t * cfg_users = config_setting_add(cfg_root, "groups", CONFIG_TYPE_GROUP);
	if (cfg_users == NULL) return CONFIG_FALSE;

	config_setting_t * cfg_default_uid = config_setting_add(cfg_users, "default_gid", CONFIG_TYPE_INT);
	if (cfg_default_uid == NULL) return CONFIG_FALSE;
	return config_setting_set_int(cfg_default_uid, id);

	return CONFIG_TRUE;
}


#ifdef __CYGWIN__
static int local_config_set_dokan(config_setting_t * cfg_root)
{
	config_setting_t * cfg_dokan = config_setting_add(cfg_root, "dokan", CONFIG_TYPE_GROUP);
	if (cfg_dokan == NULL) return CONFIG_FALSE;

	config_setting_t * cfg_volume_name = config_setting_add(cfg_dokan, "volume_name", CONFIG_TYPE_STRING);
	if (cfg_volume_name == NULL) return CONFIG_FALSE;
	config_setting_set_string(cfg_volume_name, "ZlomekFS");

	config_setting_t * cfg_file_system_name = config_setting_add(cfg_dokan, "file_system_name", CONFIG_TYPE_STRING);
	if (cfg_file_system_name == NULL) return CONFIG_FALSE;
	config_setting_set_string(cfg_file_system_name, "ZlomekClusterFS");

	return CONFIG_TRUE;
}
#endif

static int create_local_settings(const char * file_name)
{
	int rv;

	config_t zfs_config;
	config_init(&zfs_config);

	config_setting_t * cfg_root = config_lookup(&zfs_config, "/");
	if (cfg_root ==NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_lookup() failed\n");
		return CONFIG_FALSE;
	}


	const char * zfs_install_prefix = getenv("ZFS_INSTALL_PREFIX");
	if (zfs_install_prefix == NULL) zfs_install_prefix = "";

	const char * zfs_node_name = getenv("ZFS_NODE_NAME");
	if (zfs_node_name == NULL) zfs_node_name = "the_only_node";

	int zfs_node_id = 1;
	const char * zfs_node_id_str = getenv("ZFS_NODE_ID");
	if (zfs_node_id_str != NULL) zfs_node_id = atoi(zfs_node_id_str);

	int zfs_port = 12325;
	const char * zfs_port_str = getenv("ZFS_PORT");
	if (zfs_port_str != NULL) zfs_port = atoi(zfs_port_str);



	config_setting_t * cfg_version = config_setting_add(cfg_root, "version", CONFIG_TYPE_STRING);
	config_setting_set_string(cfg_version, "1.0");

	rv =  local_config_set_local_node(cfg_root, zfs_node_name, zfs_node_id, zfs_port);
	rv = local_config_set_volumes_list(cfg_root, zfs_install_prefix);
	rv = local_config_set_system(cfg_root, 0, 1);
	rv = local_config_set_versioning(cfg_root, 0);
	rv = local_config_set_threads(cfg_root);
	rv = local_config_set_users(cfg_root, 65534);
	rv = local_config_set_groups(cfg_root, 65534);
#ifdef __CYGWIN__
	rv = local_config_set_dokan(cfg_root);
#endif

	rv =  config_write_file(&zfs_config, file_name);
	if (rv == CONFIG_FALSE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "config_write_file() failed\n");
	}

	config_destroy(&zfs_config);

	return CONFIG_TRUE;
}

int main(int argc, char * argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "gen_sample_cfg local_config_file\n"
				"\t generates sample local_config_file\n"
				"\t output can be altered by enviroment variables, see bellow:\n"
				"\t\tZFS_INSTALL_PREFIX\n"
				"\t\tZFS_NODE_NAME\n"
				"\t\tZFS_NODE_ID\n"
				"\t\tZFS_PORT\n"
				);

		return EXIT_FAILURE;
	}
	create_local_settings(argv[1]);
	return EXIT_SUCCESS;
}

