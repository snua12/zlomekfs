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

#include "system.h"
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "zfs_prot.h"

/* Hash table of nodes.  */
static htab_t node_htab;

/* Mutex for table of nodes.  */
pthread_mutex_t node_mutex;

/* Description of local node.  */
node this_node;

/* Hash function for node ID.  */
#define NODE_HASH_ID(ID) (ID)

/* Hash function for node N.  */
#define NODE_HASH(N) ((N)->id)

/* Hash function for node X.  */

static hash_t
node_hash (const void *x)
{
  return NODE_HASH ((node) x);
}

/* Compare a node X with ID *Y.  */

static int
node_eq (const void *x, const void *y)
{
  node n = (node) x;
  unsigned int id = *(unsigned int *) y;

  return n->id == id;
}

node
node_lookup (unsigned int id)
{
  return (node) htab_find_with_hash (node_htab, &id, NODE_HASH_ID (id));
}

/* Create new node with ID and NAME and insert it to hash table.  */

node
node_create (unsigned int id, char *name)
{
  node nod;
  void **slot;

  nod = (node) xmalloc (sizeof (struct node_def));
  nod->id = id;
  nod->name = xstrdup (name);
  nod->flags = 0;
  nod->conn = CONNECTION_NONE;
  nod->auth = AUTHENTICATION_NONE;
#ifdef RPC
  nod->clnt = NULL;
#endif
  nod->fd = -1;
  nod->generation = 0;

  /* Are we creating a structure describing local node?  */
  if (strcmp (name, node_name) == 0)
    {
      this_node = nod;
      nod->flags |= NODE_LOCAL;
      nod->conn = CONNECTION_FAST;
      nod->auth = AUTHENTICATION_DONE;
    }

  pthread_mutex_lock (&node_mutex);
#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   NO_INSERT);
  if (slot)
    abort ();
#endif

  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   INSERT);
  *slot = nod;
  pthread_mutex_unlock (&node_mutex);

  return nod;
}

void
node_destroy (node nod)
{
  void **slot;

  pthread_mutex_lock (&node_mutex);
  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (node_htab, slot);
  pthread_mutex_unlock (&node_mutex);

  free (nod->name);
  free (nod);
}

/* Initialize data structures in NODE.C.  */

void
initialize_node_c ()
{
  pthread_mutex_init (&node_mutex, NULL);
  node_htab = htab_create (50, node_hash, node_eq, NULL, &node_mutex);
}

/* Destroy data structures in NODE.C.  */

void
cleanup_node_c ()
{
  void **slot;

  pthread_mutex_lock (&node_mutex);
  HTAB_FOR_EACH_SLOT (node_htab, slot, node_destroy ((node) *slot));
  htab_destroy (node_htab);
  pthread_mutex_unlock (&node_mutex);
  pthread_mutex_destroy (&node_mutex);
}
