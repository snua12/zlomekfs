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
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "config.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "network.h"
#include "thread.h"
#include "zfs_prot.h"

/* Hash table of nodes, searched by ID.  */
static htab_t node_htab;

/* Hash table of nodes, searched by NAME.  */
static htab_t node_htab_name;

/* Mutex for table of nodes.  */
pthread_mutex_t node_mutex;

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
  unsigned int id = *(unsigned int *) y;

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
node_lookup (unsigned int id)
{
  node nod;

  CHECK_MUTEX_LOCKED (&node_mutex);

  nod = (node) htab_find_with_hash (node_htab, &id, HASH_NODE_ID (id));
  if (nod)
    zfsd_mutex_lock (&nod->mutex);

  return nod;
}

/* Return the node whose name is NAME.  */

node
node_lookup_name (char *name)
{
  node nod;

  CHECK_MUTEX_LOCKED (&node_mutex);

  nod = (node) htab_find_with_hash (node_htab_name, name,
				    HASH_NODE_NAME (name));
  if (nod)
    zfsd_mutex_lock (&nod->mutex);

  return nod;
}

/* Create new node with ID and NAME and insert it to hash table.  */

node
node_create (unsigned int id, char *name)
{
  node nod;
  void **slot;

  CHECK_MUTEX_LOCKED (&node_mutex);

  nod = (node) xmalloc (sizeof (struct node_def));
  nod->id = id;
  nod->name = xstrdup (name);
  nod->flags = 0;
  nod->last_connect = 0;
#ifdef RPC
  nod->clnt = NULL;
#endif
  nod->fd = -1;
  nod->generation = 0;

  /* Are we creating a structure describing local node?  */
  if (strcmp (name, node_name) == 0)
    this_node = nod;

  zfsd_mutex_init (&nod->mutex);
  zfsd_mutex_lock (&nod->mutex);

#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (node_htab, &nod->id, NODE_HASH (nod),
				   INSERT);
  *slot = nod;

#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (node_htab_name, nod->name,
				   NODE_HASH_NAME (nod), NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (node_htab_name, nod->name,
				   NODE_HASH_NAME (nod), INSERT);
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

  zfsd_mutex_unlock (&nod->mutex);
  zfsd_mutex_destroy (&nod->mutex);
  free (nod->name);
  free (nod);
}

/* Update file descriptor of node NOD to be FD with generation GENERATION.  */

void
node_update_fd (node nod, int fd, unsigned int generation)
{
  CHECK_MUTEX_LOCKED (&nod->mutex);

  if (nod->fd >= 0 && nod->fd != fd)
    {
      zfsd_mutex_lock (&network_fd_data[nod->fd].mutex);
      if (nod->generation == network_fd_data[nod->fd].generation)
	network_fd_data[nod->fd].flags = NETWORK_FD_CLOSE;
      zfsd_mutex_unlock (&network_fd_data[nod->fd].mutex);
    }

  nod->fd = fd;
  nod->generation = generation;
}

/* If node NOD is connected return true and lock NETWORK_FD_DATA[NOD->FD].MUTEX.
   This function expects NOD->MUTEX to be locked.  */

bool
node_connected_p (node nod)
{
  CHECK_MUTEX_LOCKED (&nod->mutex);

  if (nod->fd < 0)
    return false;

  zfsd_mutex_lock (&network_fd_data[nod->fd].mutex);
  if (nod->generation != network_fd_data[nod->fd].generation)
    {
      zfsd_mutex_unlock (&network_fd_data[nod->fd].mutex);
      return false;
    }

  return true;
}

/* Connect to node NOD, return open file descriptor.  */

static int
node_connect (node nod)
{
  struct addrinfo *addr, *a;
  int s = -1;
  int err;

  CHECK_MUTEX_LOCKED (&nod->mutex);

  /* Lookup the IP address.  */
  addr = NULL;
  if ((err = getaddrinfo (nod->name, NULL, NULL, &addr)) != 0)
    {
      if (addr)
	abort ();
      message (-1, stderr, "getaddrinfo(): %s\n", gai_strerror (err));
      return -1;
    }

  for (a = addr; a; a = a->ai_next)
    {
      switch (a->ai_family)
	{
	  case AF_INET:
	    if (a->ai_socktype == SOCK_STREAM
		&& a->ai_protocol == IPPROTO_TCP)
	      {
		s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		/* Connect the network socket to ZFS_PORT.  */
		((struct sockaddr_in *)a->ai_addr)->sin_port = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) >= 0)
		  goto node_connected;
	      }
	    break;

	  case AF_INET6:
	    if (a->ai_socktype == SOCK_STREAM
		&& a->ai_protocol == IPPROTO_TCP)
	      {
		s = socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		/* Connect the network socket to ZFS_PORT.  */
		((struct sockaddr_in6 *)a->ai_addr)->sin6_port
		  = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) >= 0)
		  goto node_connected;
	      }
	    break;
	}
    }

  freeaddrinfo (addr);
  message (-1, stderr, "Could not connect to %s\n", nod->name);
  close (s);
  return -1;

node_connected:
  freeaddrinfo (addr);
  network_fd_data[s].auth = AUTHENTICATION_NONE;
  network_fd_data[s].conn = CONNECTION_FAST; /* FIXME */
  message (2, stderr, "FD %d connected to %s\n", s, nod->name);
  return s;
}

/* Authenticate connection with node NOD using data of thread T.
   On success leave NETWORK_FD_DATA[NOD->FD].MUTEX lcoked.  */

static bool
node_authenticate (thread *t, node nod)
{
  auth_stage1_args args1;
  auth_stage2_args args2;
  int32_t r;

  CHECK_MUTEX_LOCKED (&nod->mutex);
  CHECK_MUTEX_LOCKED (&network_fd_data[nod->fd].mutex);

  memset (&args1, 0, sizeof (args1));
  memset (&args2, 0, sizeof (args2));

  /* FIXME: really do authentication; currently the functions are empty.  */
  args1.node.len = node_name_len;
  args1.node.str = node_name;
  r = zfs_proc_auth_stage1_client_1 (t, &args1, nod->fd);
  if (r != ZFS_OK)
    goto node_authenticate_error;
  if (!node_connected_p (nod))
    goto node_authenticate_error;

  CHECK_MUTEX_LOCKED (&network_fd_data[nod->fd].mutex);
  network_fd_data[nod->fd].auth = AUTHENTICATION_IN_PROGRESS;
  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd_data (&t->dc_reply, &network_fd_data[nod->fd]);

  r = zfs_proc_auth_stage2_client_1 (t, &args2, nod->fd);
  if (r != ZFS_OK)
    goto node_authenticate_error;
  if (!node_connected_p (nod))
    goto node_authenticate_error;

  CHECK_MUTEX_LOCKED (&network_fd_data[nod->fd].mutex);
  network_fd_data[nod->fd].auth = AUTHENTICATION_FINISHED;
  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd_data (&t->dc_reply, &network_fd_data[nod->fd]);
  return true;

node_authenticate_error:
  message (2, stderr, "not auth\n");
  network_fd_data[nod->fd].auth = AUTHENTICATION_NONE;
  network_fd_data[nod->fd].conn = CONNECTION_NONE;
  zfsd_mutex_lock (&network_fd_data[nod->fd].mutex);
  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd_data (&t->dc_reply, &network_fd_data[nod->fd]);
  close_network_fd (nod->fd);
  zfsd_mutex_unlock (&network_fd_data[nod->fd].mutex);
  nod->fd = -1;
  return false;
}

/* Check whether node NOD is connected and authenticated. If not do so.
   Return open file descriptor and leave its NETWORK_FD_DATA locked.  */

int
node_connect_and_authenticate (thread *t, node nod)
{
  int fd;

  CHECK_MUTEX_LOCKED (&nod->mutex);

  if (!node_connected_p (nod))
    {
      time_t now;

      /* Do not try to connect too often.  */
      now = time (NULL);
      if (now - nod->last_connect < NODE_CONNECT_VISCOSITY)
	{
	  t->retval = ZFS_COULD_NOT_CONNECT;
	  return -1;
	}
      nod->last_connect = now;

      fd = node_connect (nod);
      if (fd < 0)
	{
	  t->retval = ZFS_COULD_NOT_CONNECT;
	  return -1;
	}
      add_fd_to_active (fd);
      node_update_fd (nod, fd, network_fd_data[fd].generation);

      if (!node_authenticate (t, nod))
	{
	  t->retval = ZFS_COULD_NOT_AUTH;
	  return -1;
	}
    }
  else
    fd = nod->fd;

  return fd;
}

/* Initialize data structures in NODE.C.  */

void
initialize_node_c ()
{
  zfsd_mutex_init (&node_mutex);
  node_htab = htab_create (50, node_hash, node_eq, NULL, &node_mutex);
  node_htab_name = htab_create (50, node_hash_name, node_eq_name, NULL,
				&node_mutex);
}

/* Destroy data structures in NODE.C.  */

void
cleanup_node_c ()
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
