/* Configuration.
   Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef CONFIG_H
#define CONFIG_H

#include "system.h"
#include <stdio.h>
#include <netinet/in.h>
#include "pthread.h"
#include "memory.h"

/* File used to communicate with kernel.  */
extern string kernel_file_name;

/* Directory with node configuration.  */
extern string node_config;

/* RW-lock for access to configuration.  */
extern pthread_rwlock_t lock_config;

extern void set_node_name (void);
extern void set_default_uid_gid (void);
extern bool read_config_file (const char *file);
extern bool read_cluster_config (void);
extern void cleanup_config_c (void);

#endif
