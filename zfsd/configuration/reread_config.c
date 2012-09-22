/**
 *  \file reread_config.c 
 * 
 *  \brief Implements fuction for reread cluster config.
 *  \author Ales Snuparek (refactoring and partial rewrite and libconfig integration)
 *  \author Josef Zlomek (initial experimental implementation)
 *
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

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

#include "system.h"
#include  <string.h>
#include <libconfig.h>
#include "memory.h"
#include "reread_config.h"
#include "zfs-prot.h"
#include "dir.h"
#include "node.h"
#include "config_volume.h"
#include "local_config.h"
#include "user-group.h"
#include "config_user_group.h"
#include "thread.h"
#include "configuration.h"
#include "shared_config.h"
#include "zfsio.h"

/*! \brief First of the chain of requests for rereading configuration. */
static reread_config_request reread_config_first;

/*! \brief Last of the chain of requests for rereading configuration. */
static reread_config_request reread_config_last;

/*! \brief Read list of nodes from CONFIG_DIR/node_list.
 *  \return true on success
 *  \return false on fail*/
bool read_node_list(zfs_fh * config_dir)
{
	dir_op_res node_list_res;
	int32_t r;

	r = zfs_extended_lookup(&node_list_res, config_dir, "node_list");
	if (r != ZFS_OK)
		return false;

	zfs_file * file = zfs_fopen(&node_list_res.file);
	if (file == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read shared node list.\n");
		return false;
	}

	config_t config;
	config_init(&config);
	int rv;
	rv = config_read(&config, zfs_fdget(file));
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared node list.\n");
		zfs_fclose(file);
		return false;

	}

	rv = read_node_list_shared_config(&config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared node lis.t\n");
	}

	config_destroy(&config);
	zfs_fclose(file);

	return (rv == CONFIG_TRUE);
}

/*! \brief Reread list of nodes.
 *  \return true on success
 *  \return false on fail*/
static bool reread_node_list(void)
{
	dir_op_res config_dir_res;
	int32_t r;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return false;

	mark_all_nodes();

	if (!read_node_list(&config_dir_res.file))
		return false;

	if (this_node == NULL || this_node->marked)
		return false;

	destroy_marked_volumes();
	destroy_marked_nodes();

	return true;
}

/*! \brief Reread list of volumes.
 *  \return true on success
 *  \return false on fail*/
static bool reread_volume_list(void)
{
	dir_op_res config_dir_res;
	int32_t r;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return false;

	mark_all_volumes();

	if (!read_volume_list(&config_dir_res.file))
		return false;

	destroy_marked_volumes();

	return true;
}

/*! \brief Reread user mapping for node SID.
 *  \return true on success
 *  \return false on fail*/
static bool reread_user_mapping(uint32_t sid)
{
	dir_op_res config_dir_res;
	dir_op_res user_dir_res;
	int32_t r;
	node nod;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return true;

	r = zfs_extended_lookup(&user_dir_res, &config_dir_res.file, "user");
	if (r != ZFS_OK)
		return true;

	if (sid == 0)
		nod = NULL;
	else if (sid == this_node->id)
		nod = this_node;
	else
		return true;

	mark_user_mapping(nod);

	if (!read_user_mapping(&user_dir_res.file, sid))
		return false;

	destroy_marked_user_mapping(nod);

	return true;
}

/*! \brief Reread list of users.
 *  \return true on success
 *  \return false on fail*/
static bool reread_user_list(void)
{
	dir_op_res config_dir_res;
	int32_t r;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return false;

	mark_all_users();

	if (!read_user_list(&config_dir_res.file))
		return false;

	destroy_marked_user_mapping(this_node);

	destroy_marked_users();

	return true;
}

/*! \brief Reread list of groups.
 *  \return true on success
 *  \return false on fail*/
static bool reread_group_list(void)
{
	dir_op_res config_dir_res;
	int32_t r;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return false;

	mark_all_groups();

	if (!read_group_list(&config_dir_res.file))
		return false;

	destroy_marked_group_mapping(this_node);
	destroy_marked_groups();

	return true;
}

/*! \brief Reread group mapping for node SID.
 *  \return true on success
 *  \return false on fail*/
static bool reread_group_mapping(uint32_t sid)
{
	dir_op_res config_dir_res;
	dir_op_res group_dir_res;
	int32_t r;
	node nod;

	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
		return true;

	r = zfs_extended_lookup(&group_dir_res, &config_dir_res.file, "group");
	if (r != ZFS_OK)
		return true;

	if (sid == 0)
		nod = NULL;
	else if (sid == this_node->id)
		nod = this_node;
	else
		return true;

	if (nod)
	{
		zfsd_mutex_lock(&nod->mutex);
		mark_group_mapping(nod);
		zfsd_mutex_unlock(&nod->mutex);
	}
	else
		mark_group_mapping(nod);

	if (!read_group_mapping(&group_dir_res.file, sid))
		return false;

	destroy_marked_group_mapping(nod);

	return true;
}

/*! \brief Reread configuration file RELATIVE_PATH.
 *  \return true on success
 *  \return false on fail*/
bool reread_config_file(string * relative_path)
{
	char *str = relative_path->str;

	if (*str != '/')
		return true;

	str++;

	if (strncmp(str, "node_list", 10) == 0)
	{
		return reread_node_list();
	}
	else if (strncmp(str, "volume_list", 11) == 0)
	{
		return reread_volume_list();
	}
	else if (strncmp(str, "user", 4) == 0)
	{
		str += 4;
		if (*str == '/')
		{
			str++;
			if (strncmp(str, "default", 8) == 0)
			{
				return reread_user_mapping(0);
			}
			else if (strcmp(str, this_node->name.str) == 0)
			{
				return reread_user_mapping(this_node->id);
			}
		}
		else if (strncmp(str, "_list", 6) == 0)
		{
			return reread_user_list();
		}
	}
	else if (strncmp(str, "group", 5) == 0)
	{
		str += 5;
		if (*str == '/')
		{
			str++;
			if (strncmp(str, "default", 8) == 0)
			{
				return reread_group_mapping(0);
			}
			else if (strcmp(str, this_node->name.str) == 0)
			{
				return reread_group_mapping(this_node->id);
			}
		}
		else if (strncmp(str, "_list", 6) == 0)
		{
			return reread_group_list();
		}
	}

	return true;
}

/*! \brief Reread local info about volumes.
 *  \param path Path where local configuration is stored.
 *  \return true on success
 *  \return false on fail*/
bool reread_local_volume_info(const char * path)
{
	mark_all_volumes();

	config_t config;
	config_init(&config);

	int rv = config_read_file(&config, path);
	if (rv != CONFIG_TRUE)
	{
		config_destroy(&config);
		return false;
	}


	rv = read_volumes_local_config_from_file(path, true);

	config_destroy(&config);

	if (rv != CONFIG_TRUE)
	{
		return false;
	}


	delete_dentries_of_marked_volumes();

	return true;
}

/*! \brief Add request to reread config file DENTRY to queue.
 *  \return true on success
 *  \return false on fail*/
void add_reread_config_request_dentry(internal_dentry dentry)
{
	string relative_path;
	thread *t;

	build_relative_path(&relative_path, dentry);

	t = (thread *) pthread_getspecific(thread_data_key);
#ifdef ENABLE_CHECKING
	if (t == NULL)
		zfsd_abort();
#endif

	add_reread_config_request(&relative_path, t->from_sid);
}


/*! \brief Add a request to reread config into queue
 *  \return true on success
 *  \return false on fail*/
void add_reread_config_request(string * relative_path, uint32_t from_sid)
{
	reread_config_request req;

	if (get_thread_state(&zfs_config.config_reader_data) != THREAD_IDLE)
		return;

	zfsd_mutex_lock(&reread_config_mutex);
	req = (reread_config_request) pool_alloc(reread_config_pool);
	req->next = NULL;
	req->relative_path = *relative_path;
	req->from_sid = from_sid;

	// append to queue
	if (reread_config_last)
		reread_config_last->next = req;
	else
		reread_config_first = req;
	reread_config_last = req;

	zfsd_mutex_unlock(&reread_config_mutex);

	semaphore_up(&zfs_config.config_sem, 1);
}

/*! \brief Add request to reread config file PATH on volume VOL to queue.
 *  \return true on success
 *  \return false on fail*/
void add_reread_config_request_local_path(volume vol, string * path)
{
	string relative_path;
	thread *t;

	local_path_to_relative_path(&relative_path, vol, path);

	t = (thread *) pthread_getspecific(thread_data_key);
#ifdef ENABLE_CHECKING
	if (t == NULL)
		zfsd_abort();
#endif

	add_reread_config_request(&relative_path, t->from_sid);
}

/*! \brief Get a request to reread config from queue
 *   and store the relative path of
 *   the file to be reread to RELATIVE_PATH and the node ID which the request
 *   came from to FROM_SID.
 *  \return true on success
 *  \return false on fail*/
bool get_reread_config_request(string * relative_path, uint32_t * from_sid)
{
	reread_config_request req;

	zfsd_mutex_lock(&reread_config_mutex);
	if (reread_config_first == NULL)
	{
		zfsd_mutex_unlock(&reread_config_mutex);
		return false;
	}

	*relative_path = reread_config_first->relative_path;
	*from_sid = reread_config_first->from_sid;

	req = reread_config_first;
	reread_config_first = reread_config_first->next;
	if (reread_config_first == NULL)
		reread_config_last = NULL;

	// free request's memory
	pool_free(reread_config_pool, req);

	zfsd_mutex_unlock(&reread_config_mutex);
	return true;
}

