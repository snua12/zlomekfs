/* Node functions.
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

#ifndef NODE_H
#define NODE_H

#include "system.h"
#include <netdb.h>
#include <rpc/rpc.h>
#include "pthread.h"
#include "thread.h"

/* Node description.  */
typedef struct node_def
{
  pthread_mutex_t mutex;
  unsigned int id;		/* ID of the node */
  char *name;			/* name of the node */
				/* public key */
  int flags;			/* see NODE_* below */
  time_t last_connect;		/* last attemp to connect to node */
  int fd;			/* file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
#ifdef RPC
  CLIENT *clnt;			/* RPC client */
#endif
} *node;

/* Node flags.  */
#define NODE_DELETE		1	/* the node should be deleted from
					   memory datastructures  */

/* Mutex for table of nodes.  */
extern pthread_mutex_t node_mutex;

/* Description of local node.  */
extern node this_node;

/* Function prototypes.  */
extern node node_lookup (unsigned int id);
extern node node_lookup_name (char *name);
extern node node_create (unsigned int id, char *name);
extern void node_destroy (node nod);
extern void node_update_fd (node nod, int fd, unsigned int generation);
extern bool node_connected_p (node nod);
extern int node_connect_and_authenticate (thread *t, node nod);
extern void initialize_node_c ();
extern void cleanup_node_c ();

#endif
