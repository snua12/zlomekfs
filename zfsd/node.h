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

/* Connection status.  */
typedef enum connection_status_def
{
  CONNECTION_NONE = 0,
  CONNECTION_SLOW,
  CONNECTION_FAST
} connection_status;

/* Status of authentication.  */
typedef enum authentication_status_def
{
  AUTHENTICATION_NONE = 0,
  AUTHENTICATION_IN_PROGRESS,
  AUTHENTICATION_DONE
} authentication_status;

/* Node description.  */
typedef struct node_def
{
  unsigned int id;		/* ID of the node */
  char *name;			/* name of the node */
  struct sockaddr_in addr;	/* address */
				/* public key */
  int flags;			/* see NODE_* below */
  connection_status conn;	/* connection status */
  authentication_status auth;	/* authentication status */
  CLIENT *clnt;			/* RPC client */
} *node;

/* Node flags.  */
#define NODE_DELETE		1	/* the node should be deleted from
					   memory datastructures  */
#define NODE_LOCAL		2	/* the node is local node */
#define NODE_ADDR_RESOLVED	4	/* the address of node is resolved  */

/* Function prototypes.  */
extern node node_lookup (unsigned int id);
extern node node_create (unsigned int id, char *name);
extern void initialize_node_c ();
extern void cleanup_node_c ();

#endif
