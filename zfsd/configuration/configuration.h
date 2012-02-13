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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "system.h"
#include <stdio.h>
#include <inttypes.h>
#include <libconfig.h>
#include "pthread-wrapper.h"
#include "memory.h"
#include "semaphore.h"
#include "thread.h"
#include "fh.h"
#include "zfs_config.h"
#include "cluster_config.h"

/* ! Mutex protecting the reread_config chain and alloc pool.  */
extern pthread_mutex_t reread_config_mutex;

/* ! Alloc pool for allocating nodes of reread config chain.  */
extern alloc_pool reread_config_pool;

extern void initialize_config_c(void);
extern void cleanup_config_c(void);

// workaroung libconfig API change
#if LIBCONFIG_VER_MAJOR >= 1 && LIBCONFIG_VER_MINOR >=4
#define LIBCONFIG_INT_TYPECAST int
#else
#define LIBCONFIG_INT_TYPECAST long
#endif

#endif
