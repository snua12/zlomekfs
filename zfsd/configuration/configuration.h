/**
 *  \file configuration.h
 * 
 *  \brief Implements function for intializing configuratin module
 *  \author Ales Snuparek (refactoring and partial rewrite and libconfig integration)
 *  \author Josef Zlomek (initial experimental implementation)
 *
 */

/* Copyright (C) 2003, 2004, 2010, 2012 Josef Zlomek, Rastislav Wartiak,
   Ales Snuparek

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

/*! \brief Mutex protecting the reread_config chain and alloc pool.  */
extern pthread_mutex_t reread_config_mutex;

/*! \brief Alloc pool for allocating nodes of reread config chain.  */
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

/*! \page zfs-configuration ZlomekFS configuration
 * Filesystem zlomekFS has local and cluster (shared) configuration.
 * Big part of zlomekFS configuration is during runtime keep in
 * global structure.
 * \section local_config Local configuration
 * Local configuration is saved to the local file system.
 * In this configuration are stored: node name, location of volume caches,
 * default user, default group, versioning configuration, threading configuration,
 * system specific configuration.
 * This configuration is stored in single text file.
 * This file is read during zlomekFS startup by libconfig.
 * \see read_local_config_from_file
 * 
 * \section cluster_config Cluster configuration
 * Cluster Configurations are saved on the partition zlomekFS.
 * It is stored in multiple files.
 * In this configuration are stored: volume list, node list, node hierarchy,
 * user and group mappings.
 * Cluster configuration is read during startup and every time, when one of
 * configuration file is modified.
 * Update of this configuration is realized by special thread and file is readed
 * by libconfig.
 * \see  read_cluster_config
 * \section zfs_config Structure with global configuration
 * All parts of zlomekFS local configuration are keep in global structure. 
 * \see zfs_config
 */
