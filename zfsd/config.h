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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "system.h"
#include <stdio.h>
#include <netinet/in.h>
#include "zfs_prot.h"
#include "log.h"

/* The host name of local node.  */
extern char *node_name;

/* Directory with node configuration. */
extern char *node_config;

/* Direcotry with cluster configuration. */
extern char *cluster_config;

/* Node description.  */
typedef struct node_def
{
  char *name;			/* name of the node */
  struct sockaddr_in addr;	/* address */
  				/* public key */
  int flags;			/* see NODE_* below */
} *node;
#define NODE_LOCAL		1	/* the node is local node */
#define NODE_ADDR_RESOLVED	2	/* the address of node is resolved  */

/* Volume description.  */
typedef struct volume_def
{
  char *name;		/* name of the volume */
  node master;		/* master node for the volume */
  char *mountpoint;	/* "mountpoint" for the volume on the cluster fs */
  int flags;		/* see VOLUME_* below */
  
  char *localpath;
  uint64_t size_limit;
  char data[1];
} *volume;

#define VOLUME_LOCAL	1	/* this volume is located on local node */
#define VOLUME_COPY	2	/* this is a copy of a volume */

extern int read_config(const char *file);

#endif 
