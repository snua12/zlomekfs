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

#include "zfs_config.h"

zfs_configuration zfs_config = 
{
	//.config_reader_data = ,
	//.config_sem = ,
	.config_node = NULL,
	.mlock_zfsd = true,
	.local_config_path = "/etc/zfs/zfs.conf",
	.default_node_uid = (uint32_t) - 1,
	.default_node_gid = (uint32_t) - 1,
	.this_node = {
		.node_id = (uint32_t) - 1,
		.node_name = STRING_INVALID_INITIALIZER,
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
#ifdef ENABLE_VERSIONS
	.versions = {
		.versioning = false,
		.verdisplay = false,
		.retention_age_min = -1,
		.retention_age_max = -1,
		.retention_num_min =  -1,
		.retention_num_max = -1
	}
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

string * get_this_node_name(void)
{
	return &zfs_config.this_node.node_name;
}




