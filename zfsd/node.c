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

#include "system.h"
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "user-group.h"

/* Hash table of nodes, searched by ID.  */
static htab_t node_htab;

/* Hash table of nodes, searched by NAME.  */
static htab_t node_htab_name;

/* Mutex for table of nodes.  */
pthread_mutex_t node_mutex;

/* The host name of local node.  */
string node_name;

/* Description of local node.  */
node this_node;

/* Hash function for node ID.  */
#define HASH_NODE_ID(ID) (ID)

/* Hash function for node NODE, computed from ID.  */
#define NODE_HASH(NODE) ((NODE)->id)

/* Hash function for node name.  */
#define HASH_NODE_NAME(NAME) crc32_string (NAME)

/* Hash function for node NODE, computed from its name.  */
#define NODE_HASH_NAME(NODE) HASH_NODE_NAME ((NODE)->name)

/* Hash function for node X, computed from ID.  */

static hash_t
node_hash (const void *x)
{
  return NODE_HASH ((node) x);
}

/* Hash function for node X, computed from node name.  */

static hash_t
node_hash_name (const void *x)
{
  return NODE_HASH_NAME ((node) x);
}

/* Compare a node X with ID *Y.  */

static int
node_eq (const void *x, const void *y)
{
  node nod = (node) x;
  uint32_t id = *(uint32_t *) y;

  return nod->id == id;
}

/* Compare a name of node X with string Y.  */

static int
node_eq_name (const void *x, const void *y)
{
  node nod = (node) x;
  char *s = (char *) y;

  return (strcmp (nod->name, s) == 0);
}

/* Return the node with id ID.  */

node
node_lookup (uint32_t id)
{
  node nod;

  zfsd_mutex_lock (&node_mutex);
  nod = (node) htab_find_with_hash (node_htab, &id, HASH_NODE_ID (id));
  if (nod)
    zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);

  return nod;
}

/* Return the node whose name is NAME.  */

node
node_lookup_name (char *name)
{
  node nod;

  zfsd_mutex_lock (&node_mutex);
  nod = (node) htab_find_with_hash (node_htab_name, name,
				    HASH_NODE_NAME (name));
  if (nod)
    zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);

  return nod;
}

/* Create new node with ID and NAME and insert it to hash table.  */

node
node_create (uint32_t id, char *name)
{
  node nod;
  void **slot;

  CHECK_MUTEX_LOCKED (&node_mutex);

  nod = (node) xmalloc (sizeof (struct node_def));
  nod->id = id;
  nod->name = xstrdup (name);
  nod->flags = 0;
  nod->last_connect = 0;
  nod->fd = -1;
  nod->generation = 0;
  nod->map_uid_to_node = NULL;
  nod->map_uid_to_zfs = NULL;
  nod->map_gid_to_node = NULL;
  nod->map_gid_to_zfs = NULL;

  /* Are we creating a structure describing local node?  */
  if (strcmp (name, node_name.str) == 0)
    {
      this_node = nod;
      nod->map_uid_to_node = htab_create (5, map_id_to_node_hash,
					  map_id_to_node_eq, NULL,
					  &nod->mutex);
      nod->map_uid_to_zfs = htab_create (5, map_id_to_zfs_hash,
					 map_id_to_zfs_eq, NULL,
					 &nod->mutex);
      nod->map_gid_to_node = htab_create (5, map_id_to_node_hash,
					  map_id_to_node_eq, NULL,
					  &nod->mutex);
      nod->map_gid_to_zfs = htab_create (5, map_id_to_zfs_hash,
					 map_id_to_zfs_eq, NULL,
					 &nod->mutex);
    }

  zfsd_mutex_init (&nod->mutex);
  zfsd_mutex_lock (&nod->mutex);

  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = nod;

  slot = htab_find_slot_with_hash (node_htab_name, nod->name,
				   NODE_HASH_NAME (nod), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = nod;

  return nod;
}

/* Destroy node NOD and free memory associated with it.
   This function expects node_mutex to be locked.  */

void
node_destroy (node nod)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&node_mutex);
  CHECK_MUTEX_LOCKED (&nod->mutex);

  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (node_htab, slot);

  slot = htab_find_slot_with_hash (node_htab_name, nod->name,
				   NODE_HASH_NAME (nod), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (node_htab_name, slot);

  if (nod->map_uid_to_node)
    {
      user_mapping_destroy_all (nod);
      htab_destroy (nod->map_uid_to_node);
      htab_destroy (nod->map_uid_to_zfs);

      group_mapping_destroy_all (nod);
      htab_destroy (nod->map_gid_to_node);
      htab_destroy (nod->map_gid_to_zfs);
    }

  zfsd_mutex_unlock (&nod->mutex);
  zfsd_mutex_destroy (&nod->mutex);
  free (nod->name);
  free (nod);
}

/* Initialize data structures in NODE.C.  */

void
initialize_node_c (void)
{
  zfsd_mutex_init (&node_mutex);
  node_htab = htab_create (50, node_hash, node_eq, NULL, &node_mutex);
  node_htab_name = htab_create (50, node_hash_name, node_eq_name, NULL,
				&node_mutex);
}

/* Destroy data structures in NODE.C.  */

void
cleanup_node_c (void)
{
  void **slot;

  zfsd_mutex_lock (&node_mutex);
  HTAB_FOR_EACH_SLOT (node_htab, slot,
    {
      node nod = (node) *slot;

      zfsd_mutex_lock (&nod->mutex);
      node_destroy (nod);
    });
  htab_destroy (node_htab);
  htab_destroy (node_htab_name);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_destroy (&node_mutex);
}
