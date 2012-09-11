/* ! \file \brief Configuration.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak

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

	/* ! Versions displayed in readdir.  */
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
	/* ! ID of this node.  */
	uint32_t node_id;

	/* ! The name of local node.  */
	string node_name;

	/* ! The host name of local node.  */
	string host_name;

	/* ! The port of the local service */
	uint16_t host_port;
} zfs_config_node;

typedef struct zfs_config_metadata_def
{
	/* ! Depth of directory tree for saving metadata about files.  */
	unsigned int metadata_tree_depth;
} zfs_config_metadata;

typedef struct zfs_config_threads_def
{
	/* ! Limits for number of network threads.  */
	thread_limit network_thread_limit;

	/* ! Limits for number of kernel threads.  */
	thread_limit kernel_thread_limit;

	/* ! Limits for number of update threads.  */
	thread_limit update_thread_limit;
} zfs_config_threads;

typedef struct zfs_configuration_def
{
	/* ! Data for config reader thread.  */
	thread config_reader_data;

	/* ! Semaphore for managing the reread request queue.  */
	semaphore config_sem;

	/* ! mlockall() zfsd . */
	bool mlock_zfsd;

	/* ! local path to local config */
	const char * local_config_path;

	/* ! mount point of zfsd */
	char mountpoint[ZFS_MAXPATHLEN];
	
	/* ! default node uid */
	uint32_t default_node_uid;

	/* ! default node gid */
	uint32_t default_node_gid;

	/* ! local node configuration */
	zfs_config_node this_node;

	/* ! config node configuration */
	zfs_config_node config_node;

	/* ! metadata config */
	zfs_config_metadata metadata;

	/* ! threads config */
	zfs_config_threads threads;

#ifdef ENABLE_VERSIONS
	/* ! versioning config */
	zfs_config_versions versions;
#endif

#ifdef HAVE_DOKAN
	zfs_config_dokan dokan;
#endif
} zfs_configuration;

extern zfs_configuration zfs_config;


void set_local_config_path(const char * path);
const char * get_local_config_path(void);

void set_mountpoint(const char * path);
const char * get_mountpoint(void);


string * get_this_node_name(void);

void set_default_uid_gid(void);

bool set_default_uid(const char *name);

bool set_default_gid(const char *name);

uint32_t get_default_node_uid(void);

uint32_t get_default_node_gid(void);

#ifdef HAVE_DOKAN
uint32_t get_default_file_mode(void);

uint32_t get_default_directory_mode(void);
#endif

#ifdef __cplusplus
}
#endif

#endif


