/* Node functions.
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

#ifndef NODE_H
#define NODE_H

#include "system.h"
#include <inttypes.h>
#include <netdb.h>
#include "pthread.h"
#include "hashtab.h"

/* Node description.  */
typedef struct node_def
{
#ifdef ENABLE_CHECKING
  long unused0;
  long unused1;
#endif

  pthread_mutex_t mutex;
  uint32_t id;			/* ID of the node */
  string name;			/* name of the node */
  string host_name;		/* DNS name or IP address of the node */
  time_t last_connect;		/* last attemp to connect to node */
  int fd;			/* file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
  bool marked;			/* Is the node marked?  */

  /* Tables for mapping between ZFS IDs and node IDs.  */
  htab_t map_uid_to_node;
  htab_t map_uid_to_zfs;
  htab_t map_gid_to_node;
  htab_t map_gid_to_zfs;
} *node;

/* Predefined node IDs.  */
#define NODE_NONE 0		/* ID for non-existing node, used as SID
				   in file handle of virtual directory.  */

/* Mutex for table of nodes.  */
extern pthread_mutex_t node_mutex;

/* ID of this node.  */
extern uint32_t this_node_id;

/* The name of local node.  */
extern string node_name;

/* The host name of local node.  */
extern string node_host_name;

/* Description of local node.  */
extern node this_node;

/* Function prototypes.  */
extern node node_lookup (uint32_t id);
extern node node_lookup_name (string *name);
extern node node_create (uint32_t id, string *name, string *host_name);
extern node node_create_wrapper (uint32_t id, char *name, char *host_name);
extern node try_create_node (uint32_t id, string *name, string *host_name);
extern void node_destroy (node nod);
extern void mark_all_nodes (void);
extern void destroy_invalid_nodes (void);
extern void initialize_node_c (void);
extern void cleanup_node_c (void);

#endif
