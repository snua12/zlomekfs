/*! \file
    \brief Configuration.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

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

/*! Data for config reader thread.  */
thread config_reader_data;

/*! Semaphore for managing the reread request queue.  */
semaphore config_sem;

/*! Directory with local node configuration. */

/*! Node which the local node should fetch the global configuration from.  */
char *config_node;

//TODO: ugly
extern string local_config;

#ifdef ENABLE_VERSIONS
/*! Versioning enabled.  */
bool versioning = false;

/*! Versions displayed in readdir.  */
bool verdisplay = false;

/* Age retention interval.  */
int retention_age_min = -1;
int retention_age_max = -1;

/* Number of versions retention interval.  */
int retention_num_min = -1;
int retention_num_max = -1;
#endif

/*! mlockall() zfsd  .*/
bool mlock_zfsd;



/*! Alloc pool for allocating nodes of reread config chain.  */
alloc_pool reread_config_pool;

/*! Mutex protecting the reread_config chain and alloc pool.  */
pthread_mutex_t reread_config_mutex;




/*! Add request to reread config file DENTRY to queue.  */

void
add_reread_config_request_dentry (internal_dentry dentry)
{
  string relative_path;
  thread *t;

  build_relative_path (&relative_path, dentry);

  t = (thread *) pthread_getspecific (thread_data_key);
#ifdef ENABLE_CHECKING
  if (t == NULL)
    abort ();
#endif

  add_reread_config_request (&relative_path, t->from_sid);
}

/*! Add request to reread config file PATH on volume VOL to queue.  */

void
add_reread_config_request_local_path (volume vol, string *path)
{
  string relative_path;
  thread *t;

  local_path_to_relative_path (&relative_path, vol, path);

  t = (thread *) pthread_getspecific (thread_data_key);
#ifdef ENABLE_CHECKING
  if (t == NULL)
    abort ();
#endif

  add_reread_config_request (&relative_path, t->from_sid);
}


/*! Initialize data structures in CONFIG.C.  */

void
initialize_config_c (void)
{
  zfsd_mutex_init (&reread_config_mutex);
  semaphore_init (&config_sem, 0);

  reread_config_pool
    = create_alloc_pool ("reread_config_pool",
			 sizeof (struct reread_config_request_def), 1022,
			 &reread_config_mutex);
}

/*! Destroy data structures in CONFIG.C.  */

void
cleanup_config_c (void)
{
  zfsd_mutex_lock (&reread_config_mutex);
#ifdef ENABLE_CHECKING
  if (reread_config_pool->elts_free < reread_config_pool->elts_allocated)
    message (LOG_WARNING, FACILITY_CONFIG, "Memory leak (%u elements) in reread_config_pool.\n",
	     reread_config_pool->elts_allocated
	     - reread_config_pool->elts_free);
#endif
  free_alloc_pool (reread_config_pool);
  zfsd_mutex_unlock (&reread_config_mutex);
  zfsd_mutex_destroy (&reread_config_mutex);
  semaphore_destroy (&config_sem);

  if (node_name.str)
    free (node_name.str);
  free (local_config.str);
}
