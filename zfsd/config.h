/* Configuration.
   Copyright (C) 2003 Josef Zlomek

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

#ifndef _CONFIG_H
#define _CONFIG_H

#include "system.h"
#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>

/* The host name of local node.  */
extern char *node_name;

/* Directory with node configuration.  */
extern char *node_config;

/* Direcotry with cluster configuration.  */
extern char *cluster_config;

/* RW-lock for access to configuration.  */
extern pthread_rwlock_t lock_config;

extern int read_config (const char *file);

#endif
