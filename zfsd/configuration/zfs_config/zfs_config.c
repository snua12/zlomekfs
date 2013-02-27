/**
 *  \file zfs_config.c
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

#include "zfs_config.h"
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "user-group.h"
#include "semaphore.h"
#include "zfs-prot.h"
#include "metadata.h"


/*! \brief structure with global zlomekFS configuration */
zfs_configuration zfs_config = 
{
	.config_reader_data = {.mutex = ZFS_MUTEX_INITIALIZER},
	.config_sem = ZFS_SEMAPHORE_INITIALIZER(0),
	.mlock_zfsd = true,
#ifdef __ANDROID__
	.local_config_path = "/data/misc/zfsd/etc/zfsd/zfsd.conf",
#else
	.local_config_path = "/etc/zfsd/zfsd.conf",
#endif
	.mountpoint = "",
	.default_node_uid = (uint32_t) - 1,
	.default_node_gid = (uint32_t) - 1,
	.this_node = {
		.node_id = (uint32_t) - 1,
		.node_name = STRING_INVALID_INITIALIZER,
		.host_port = ZFS_PORT,
	},
	.config_node = {
		.node_id = (uint32_t) - 1,
		.node_name = STRING_INVALID_INITIALIZER,
		.host_name = STRING_INVALID_INITIALIZER,
		.host_port = ZFS_PORT,
	},
	.metadata = {
		.metadata_tree_depth = 1,
	},
	.threads = {
		.network_thread_limit = {
			.max_total = 8,
			.min_spare = 2,
			.max_spare = 4
		},
		.kernel_thread_limit = {
			.max_total = 4,
			.min_spare = 1,
			.max_spare = 2 
		},
		.update_thread_limit = {
			.max_total = 4,
			.min_spare = 1,
			.max_spare = 2
		},
	},
#ifdef ENABLE_CLI
	.cli = {
		.telnet_port = 12121,
	},
#endif
#ifdef ENABLE_VERSIONS
	.versions = {
		.versioning = false,
		.verdisplay = false,
		.retention_age_min = -1,
		.retention_age_max = -1,
		.retention_num_min =  -1,
		.retention_num_max = -1
	},
#endif
#ifdef HAVE_DOKAN
	.dokan = {
		.volume_name = STRING_INVALID_INITIALIZER,
		.file_system_name = STRING_INVALID_INITIALIZER,
		.file_mode = 0644, //rw-r--r--
		.directory_mode = 0755 //rwxr-x-r-x
	},
#endif
};

void set_local_config_path(const char * path)
{
	zfs_config.local_config_path = path;
}

const char * get_local_config_path(void)
{
	return zfs_config.local_config_path;
}

void set_mountpoint(const char * path)
{
	snprintf(zfs_config.mountpoint, sizeof(zfs_config.mountpoint) - 1, "%s", path);
}

const char * get_mountpoint(void)
{
	return zfs_config.mountpoint;
}

string * get_this_node_name(void)
{
	return &zfs_config.this_node.node_name;
}

/*! Set default node UID to UID of user NAME.  Return true on success.  */
bool set_default_uid(const char *name)
{
	struct passwd *pwd;

	pwd = getpwnam(name);
	if (!pwd)
		return false;

	zfs_config.default_node_uid = pwd->pw_uid;
	return true;
}

bool set_default_gid(const char *name)
{
	struct group *grp;

	grp = getgrnam(name);
	if (!grp)
		return false;

	zfs_config.default_node_gid = grp->gr_gid;
	return true;
}

void set_default_uid_gid(void)
{
	set_default_uid("nobody");
	if (!set_default_gid("nogroup"))
		set_default_gid("nobody");
}

uint32_t get_default_node_uid(void)
{
	return zfs_config.default_node_uid;
}

/*! \brief returns default guid for local node */
uint32_t get_default_node_gid(void)
{
	return zfs_config.default_node_gid;
}

/*! \brief returns metadata tree depth */
uint32_t get_metadata_tree_depth(void)
{
	return zfs_config.metadata.metadata_tree_depth;
}

/*! \brief set metadata tree depth */
bool set_metadata_tree_depth(uint32_t tree_depth)
{
	if (!is_valid_metadata_tree_depth(tree_depth)) return false;

	zfs_config.metadata.metadata_tree_depth = tree_depth;
	
	return true;
}

#ifdef HAVE_DOKAN
/*! \brief return default file mode */
uint32_t get_default_file_mode(void)
{
	return zfs_config.dokan.file_mode;
}

/*! \brief return default directory mode */
uint32_t get_default_directory_mode(void)
{
	return zfs_config.dokan.directory_mode;
}
#endif

