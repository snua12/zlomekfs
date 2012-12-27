/**
 *  \file zfs_config.h
 * 
 *  \brief This file contains structure which keep zlomekFS configuration
 *  \author Ales Snuparek 
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

#ifndef ZFS_CONFIG_H
#define ZFS_CONFIG_H

#include "system.h"
#include <inttypes.h>
#include "pthread-wrapper.h"
#include "memory.h"
#include "thread.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ENABLE_VERSIONS
/* Versioning enabled.  */
typedef struct zfs_config_versions_def
{
	bool versioning;

	/*! Versions displayed in readdir.  */
	bool verdisplay;

	/* Age retention interval.  */
	int retention_age_min;
	int retention_age_max;

	/* Number of versions retention interval.  */
	int retention_num_min;
	int retention_num_max;
} zfs_config_versions;
#endif

#ifdef HAVE_DOKAN
/* Dokan specific configuration */
typedef struct zfs_config_dokan_def
{
	string volume_name;
	string file_system_name;
	uint32_t file_mode;
	uint32_t directory_mode;
} zfs_config_dokan;
#endif

typedef struct zfs_config_node_def
{
	/*! ID of this node.  */
	uint32_t node_id;

	/*! The name of local node.  */
	string node_name;

	/*! The host name of local node.  */
	string host_name;

	/*! The port of the local service */
	uint16_t host_port;
} zfs_config_node;

typedef struct zfs_config_metadata_def
{
	/*! Depth of directory tree for saving metadata about files.  */
	uint32_t metadata_tree_depth;
} zfs_config_metadata;

typedef struct zfs_config_threads_def
{
	/*! Limits for number of network threads.  */
	thread_limit network_thread_limit;

	/*! Limits for number of kernel threads.  */
	thread_limit kernel_thread_limit;

	/*! Limits for number of update threads.  */
	thread_limit update_thread_limit;
} zfs_config_threads;

typedef struct zfs_configuration_def
{
	/*! Data for config reader thread.  */
	thread config_reader_data;

	/*! Semaphore for managing the reread request queue.  */
	semaphore config_sem;

	/*! mlockall() zfsd . */
	bool mlock_zfsd;

	/*! local path to local config */
	const char * local_config_path;

	/*! mount point of zfsd */
	char mountpoint[ZFS_MAXPATHLEN];
	
	/*! default node uid */
	uint32_t default_node_uid;

	/*! default node gid */
	uint32_t default_node_gid;

	/*! local node configuration */
	zfs_config_node this_node;

	/*! config node configuration */
	zfs_config_node config_node;

	/*! metadata config */
	zfs_config_metadata metadata;

	/*! threads config */
	zfs_config_threads threads;

#ifdef ENABLE_VERSIONS
	/*! versioning config */
	zfs_config_versions versions;
#endif

#ifdef HAVE_DOKAN
	zfs_config_dokan dokan;
#endif
} zfs_configuration;

/*! \brief reference to global configuration structure */
extern zfs_configuration zfs_config;

/*! \brief set path to file with local configuration */
void set_local_config_path(const char * path);

/*! \brief returns path to file with local configuration */
const char * get_local_config_path(void);

/*! \brief sets zlomekFS mountpoint */
void set_mountpoint(const char * path);

/*! \brief returns zlomekFS mountpoint */
const char * get_mountpoint(void);

/*! \brief returns local node name */
string * get_this_node_name(void);

/*! \brief sets default uid and gid for local node */
void set_default_uid_gid(void);

/*! \brief sets default uid for local node */
bool set_default_uid(const char *name);

/*! \brief sets default guid for local node */
bool set_default_gid(const char *name);

/*! \brief returns default uid for local node */
uint32_t get_default_node_uid(void);

/*! \brief returns default guid for local node */
uint32_t get_default_node_gid(void);

/*! \brief returns metadata tree depth */
uint32_t get_metadata_tree_depth(void);

/*! \brief set metadata tree depth */
bool set_metadata_tree_depth(uint32_t tree_depth);

#ifdef HAVE_DOKAN
/*! \brief return default file mode */
uint32_t get_default_file_mode(void);

/*! \brief return default directory mode */
uint32_t get_default_directory_mode(void);
#endif

#ifdef __cplusplus
}
#endif

#endif


