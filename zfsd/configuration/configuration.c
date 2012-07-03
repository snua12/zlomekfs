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

#include "system.h"
#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include "pthread-wrapper.h"
#include "configuration.h"
#include "constant.h"
#include "log.h"
#include "memory.h"
#include "alloc-pool.h"
#include "semaphore.h"
#include "node.h"
#include "volume.h"
#include "metadata.h"
#include "user-group.h"
#include "thread.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "network.h"
#include "zfsd.h"
#include "reread_config.h"

/* ! Alloc pool for allocating nodes of reread config chain.  */
alloc_pool reread_config_pool;

/* ! Mutex protecting the reread_config chain and alloc pool.  */
pthread_mutex_t reread_config_mutex = ZFS_MUTEX_INITIALIZER;

/* ! Initialize data structures in CONFIG.C.  */

void initialize_config_c(void)
{
//	semaphore_init(&zfs_config.config_sem, 0);

	reread_config_pool
		= create_alloc_pool("reread_config_pool",
							sizeof(struct reread_config_request_def), 1022,
							&reread_config_mutex);
}

/* ! Destroy data structures in CONFIG.C.  */

void cleanup_config_c(void)
{
	zfsd_mutex_lock(&reread_config_mutex);
#ifdef ENABLE_CHECKING
	if (reread_config_pool->elts_free < reread_config_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_CONFIG,
				"Memory leak (%u elements) in reread_config_pool.\n",
				reread_config_pool->elts_allocated -
				reread_config_pool->elts_free);
#endif
	free_alloc_pool(reread_config_pool);
	zfsd_mutex_unlock(&reread_config_mutex);
	//semaphore_destroy(&zfs_config.config_sem);

	xfreestring(&zfs_config.this_node.node_name);

#ifdef HAVE_DOKAN
	xfreestring(&zfs_config.dokan.volume_name);
	xfreestring(&zfs_config.dokan.file_system_name);
#endif
}
