/* Network thread functions.
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
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "semaphore.h"
#include "data-coding.h"
#include "network.h"
#include "kernel.h"
#include "log.h"
#include "util.h"
#include "malloc.h"
#include "thread.h"
#include "zfs_prot.h"
#include "node.h"
#include "volume.h"
#include "hashtab.h"
#include "alloc-pool.h"
#include "fh.h"

/* Pool of network threads.  */
thread_pool network_pool;

/* File descriptor of the main (i.e. listening) socket.  */
static int main_socket;

/* The array of data for each file descriptor.  */
fd_data_t *fd_data_a;

/* Array of pointers to data of active file descriptors.  */
static fd_data_t **active;

/* Number of active file descriptors.  */
static int nactive;

/* Mutex protecting access to ACTIVE and NACTIVE.  */
static pthread_mutex_t active_mutex;

/* Hash function for waiting4reply_data.  */

hash_t
waiting4reply_hash (const void *xx)
{
  const waiting4reply_data *x = (waiting4reply_data *) xx;

  return WAITING4REPLY_HASH (x->request_id);
}

/* Return true when waiting4reply_data XX is data for request ID *YY.  */

int
waiting4reply_eq (const void *xx, const void *yy)
{
  const waiting4reply_data *x = (waiting4reply_data *) xx;
  const unsigned int id = *(unsigned int *) yy;

  return WAITING4REPLY_HASH (x->request_id) == id;
}

/* Initialize data for file descriptor FD and add it to ACTIVE.  */

static void
init_fd_data (int fd)
{
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&active_mutex);
  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);

#ifdef ENABLE_CHECKING
  if (fd_data_a[fd].conn != CONNECTION_NONE
      && fd_data_a[fd].conn != CONNECTION_CONNECTING)
    abort ();
  if (fd_data_a[fd].conn == CONNECTION_NONE
      && fd_data_a[fd].sid != 0)
    abort ();
  if (fd_data_a[fd].conn == CONNECTION_CONNECTING
      && fd_data_a[fd].sid == 0)
    abort ();
  if (fd_data_a[fd].auth != AUTHENTICATION_NONE)
    abort ();
#endif

  /* Set the network file descriptor's data.  */
  active[nactive] = &fd_data_a[fd];
  nactive++;
  fd_data_a[fd].fd = fd;
  fd_data_a[fd].read = 0;
  if (fd_data_a[fd].ndc == 0)
    {
      dc_create (&fd_data_a[fd].dc[0], ZFS_MAX_REQUEST_LEN);
      fd_data_a[fd].ndc++;
    }
  fd_data_a[fd].last_use = time (NULL);
  fd_data_a[fd].generation++;
  fd_data_a[fd].busy = 0;
  fd_data_a[fd].flags = 0;

  fd_data_a[fd].waiting4reply_pool
    = create_alloc_pool ("waiting4reply_data",
			 sizeof (waiting4reply_data), 30,
			 &fd_data_a[fd].mutex);
  fd_data_a[fd].waiting4reply_heap
    = fibheap_new (30, &fd_data_a[fd].mutex);
  fd_data_a[fd].waiting4reply
    = htab_create (30, waiting4reply_hash, waiting4reply_eq,
		   NULL, &fd_data_a[fd].mutex);
}

/* Add file descriptor FD to the set of active file descriptors.  */

void
add_fd_to_active (int fd)
{
  zfsd_mutex_lock (&active_mutex);
  zfsd_mutex_lock (&fd_data_a[fd].mutex);
  init_fd_data (fd);
  thread_terminate_blocking_syscall (&network_pool.main_thread,
				     &network_pool.main_in_syscall);
  zfsd_mutex_unlock (&active_mutex);
}

/* Update file descriptor of node NOD to be FD with generation GENERATION.
   ACTIVE is true when this node is creating the connection.  */

void
update_node_fd (node nod, int fd, unsigned int generation, bool active)
{
  CHECK_MUTEX_LOCKED (&nod->mutex);
  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif

  if (nod->fd < 0 || nod->fd == fd)
    {
      nod->fd = fd;
      nod->generation = generation;
    }
  else
    {
      if ((active && nod->id < this_node->id)
	  || (!active && nod->id > this_node->id))
	{
	  /* The new connection is in allowed direction.  */
	  zfsd_mutex_lock (&fd_data_a[nod->fd].mutex);
	  if (nod->generation == fd_data_a[nod->fd].generation)
	    fd_data_a[nod->fd].flags = FD_FLAG_CLOSE;
	  zfsd_mutex_unlock (&fd_data_a[nod->fd].mutex);
	  nod->fd = fd;
	  nod->generation = generation;
	}
      else
	{
	  /* The new connection is in forbidden direction.  */
	  close_network_fd (fd);
	}
    }
}

/* Wake all threads waiting for reply on file descriptor with fd_data FD_DATA
   and set return value to RETVAL.  */

void
wake_all_threads (fd_data_t *fd_data, int32_t retval)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&fd_data->mutex);

  HTAB_FOR_EACH_SLOT (fd_data->waiting4reply, slot,
    {
      waiting4reply_data *data = *(waiting4reply_data **) slot;

      data->t->retval = retval;
      htab_clear_slot (fd_data->waiting4reply, slot);
      fibheap_delete_node (fd_data->waiting4reply_heap, data->node);
      pool_free (fd_data->waiting4reply_pool, data);
      semaphore_up (&data->t->sem, 1);
    });
}

/* Close file descriptor FD and update its fd_data.  */

void
close_network_fd (int fd)
{
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);

  message (2, stderr, "Closing FD %d\n", fd);
  close (fd);
  wake_all_threads (&fd_data_a[fd], ZFS_CONNECTION_CLOSED);
  htab_destroy (fd_data_a[fd].waiting4reply);
  fibheap_delete (fd_data_a[fd].waiting4reply_heap);
  free_alloc_pool (fd_data_a[fd].waiting4reply_pool);

  fd_data_a[fd].generation++;
  fd_data_a[fd].conn = CONNECTION_NONE;
  fd_data_a[fd].auth = AUTHENTICATION_NONE;
  fd_data_a[fd].sid = 0;
  zfsd_cond_broadcast (&fd_data_a[fd].cond);
}

/* Close an active file descriptor on index I in ACTIVE.  */

static void
close_active_fd (int i)
{
  int fd = active[i]->fd;
  int j;

#ifdef ENABLE_CHECKING
  if (active[i]->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&active_mutex);
  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);

  close_network_fd (fd);
  nactive--;
  if (i < nactive)
    active[i] = active[nactive];
  for (j = 0; j < fd_data_a[fd].ndc; j++)
    dc_destroy (&fd_data_a[fd].dc[j]);
  fd_data_a[fd].ndc = 0;
  fd_data_a[fd].fd = -1;
}

/* Return true if there is a valid file descriptor attached to node NOD
   and lock NETWORK_FD_DATA[NOD->FD].MUTEX.
   This function expects NOD->MUTEX to be locked.  */

bool
node_has_valid_fd (node nod)
{
  CHECK_MUTEX_LOCKED (&nod->mutex);

  if (nod->fd < 0)
    return false;

  zfsd_mutex_lock (&fd_data_a[nod->fd].mutex);
  if (nod->generation != fd_data_a[nod->fd].generation)
    {
      zfsd_mutex_unlock (&fd_data_a[nod->fd].mutex);
      return false;
    }

#ifdef ENABLE_CHECKING
  if (fd_data_a[nod->fd].sid != nod->id)
    abort ();
#endif

  return true;
}

/* Return true if current node is connected to master of volume VOL.  */

bool
volume_master_connected (volume vol)
{
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);

  if (!node_has_valid_fd (vol->master))
    {
      zfsd_mutex_unlock (&vol->master->mutex);
      return false;
    }

  r = (fd_data_a[vol->master->fd].auth == AUTHENTICATION_FINISHED);

  zfsd_mutex_unlock (&fd_data_a[vol->master->fd].mutex);
  zfsd_mutex_unlock (&vol->master->mutex);

  return r;
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
#ifdef ENABLE_CHECKING
      if (addr)
	abort ();
#endif
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
		int flags;

		s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		flags = fcntl (s, F_GETFL);
		if (flags == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }
		if (fcntl (s, F_SETFL, flags | O_NONBLOCK) == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		/* Connect the network socket to ZFS_PORT.  */
		((struct sockaddr_in *)a->ai_addr)->sin_port = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) < 0
		    && errno != EINPROGRESS)
		  {
		    message (-1, stderr, "connect(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		if (fcntl (s, F_SETFL, flags) == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		goto node_connected;
	      }
	    break;

	  case AF_INET6:
	    if (a->ai_socktype == SOCK_STREAM
		&& a->ai_protocol == IPPROTO_TCP)
	      {
		int flags;

		s = socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		flags = fcntl (s, F_GETFL);
		if (flags == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }
		if (fcntl (s, F_SETFL, flags | O_NONBLOCK) == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		/* Connect the network socket to ZFS_PORT.  */
		((struct sockaddr_in6 *)a->ai_addr)->sin6_port
		  = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) < 0
		    && errno != EINPROGRESS)
		  {
		    message (-1, stderr, "connect(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		if (fcntl (s, F_SETFL, flags) == -1)
		  {
		    message (-1, stderr, "fcntl(): %s\n", strerror (errno));
		    close (s);
		    break;
		  }

		goto node_connected;
	      }
	    break;
	}
    }

  freeaddrinfo (addr);
  message (-1, stderr, "Could not connect to %s\n", nod->name);
  return -1;

node_connected:
  freeaddrinfo (addr);
  fd_data_a[s].conn = CONNECTION_CONNECTING;
  fd_data_a[s].auth = AUTHENTICATION_NONE;
  fd_data_a[s].sid = nod->id;
  zfsd_cond_broadcast (&fd_data_a[s].cond);
  return s;
}

/* Authenticate connection with node NOD using data of thread T.
   On success leave NETWORK_FD_DATA[NOD->FD].MUTEX lcoked.  */

static int
node_authenticate (thread *t, node nod, authentication_status auth)
{
  auth_stage1_args args1;
  auth_stage2_args args2;
  int32_t r;
  uint32_t sid;
  int fd;

  CHECK_MUTEX_LOCKED (&nod->mutex);
  CHECK_MUTEX_LOCKED (&fd_data_a[nod->fd].mutex);
#ifdef ENABLE_CHECKING
  if (fd_data_a[nod->fd].conn == CONNECTION_NONE)
    abort ();
#endif

  sid = nod->id;
  fd = nod->fd;
  zfsd_mutex_unlock (&nod->mutex);
  t->retval = ZFS_COULD_NOT_CONNECT;

again:
  zfsd_mutex_unlock (&fd_data_a[fd].mutex);

  nod = node_lookup (sid);
  if (!nod)
    return -1;

  if (!node_has_valid_fd (nod))
    {
      zfsd_mutex_unlock (&nod->mutex);
      return -1;
    }
  fd = nod->fd;
  zfsd_mutex_unlock (&nod->mutex);
  nod = NULL;

  switch (fd_data_a[fd].conn)
    {
      case CONNECTION_NONE:
	abort ();

      case CONNECTION_CONNECTING:
	while (fd_data_a[fd].conn == CONNECTION_CONNECTING)
	  {
	    zfsd_cond_wait (&fd_data_a[fd].cond,
			    &fd_data_a[fd].mutex);
	  }
	t->retval = ZFS_COULD_NOT_CONNECT;
	goto again;

      case CONNECTION_PASSIVE:
	while (fd_data_a[fd].conn == CONNECTION_PASSIVE)
	  {
	    zfsd_cond_wait (&fd_data_a[fd].cond,
			    &fd_data_a[fd].mutex);
	  }
	t->retval = ZFS_COULD_NOT_AUTH;
	goto again;

      case CONNECTION_ACTIVE:
	if (fd_data_a[fd].auth >= auth)
	  return fd;
	break;

      case CONNECTION_ESTABLISHED:
	return fd;
    }

  switch (fd_data_a[fd].auth)
    {
      case AUTHENTICATION_NONE:
	fd_data_a[fd].auth = AUTHENTICATION_Q1;
	memset (&args1, 0, sizeof (args1));
	/* FIXME: really do authentication */
	args1.node = node_name;
	r = zfs_proc_auth_stage1_client_1 (t, &args1, fd);
	if (r != ZFS_OK)
	  goto node_authenticate_error;

	nod = node_lookup (sid);
	if (!nod)
	  {
	    r = ZFS_CONNECTION_CLOSED;
	    goto node_authenticate_error;
	  }
	if (!node_has_valid_fd (nod))
	  {
	    r = ZFS_CONNECTION_CLOSED;
	    goto node_authenticate_error;
	  }
	if (fd != nod->fd)
	  {
	    if (r >= ZFS_ERROR_HAS_DC_REPLY)
	      recycle_dc_to_fd_data (&t->dc_reply, &fd_data_a[nod->fd]);
	    zfsd_mutex_unlock (&nod->mutex);
	    goto again;
	  }

	/* FIXME: really do authentication */

	zfsd_mutex_unlock (&nod->mutex);
	fd_data_a[fd].auth = AUTHENTICATION_STAGE_1;
	if (r >= ZFS_ERROR_HAS_DC_REPLY)
	  recycle_dc_to_fd_data (&t->dc_reply, &fd_data_a[fd]);
	zfsd_cond_broadcast (&fd_data_a[fd].cond);

	goto again;

      case AUTHENTICATION_Q1:
	while (fd_data_a[fd].auth == AUTHENTICATION_Q1)
	  {
	    zfsd_cond_wait (&fd_data_a[fd].cond,
			    &fd_data_a[fd].mutex);
	  }
	t->retval = ZFS_COULD_NOT_AUTH;
	goto again;

      case AUTHENTICATION_STAGE_1:
	fd_data_a[fd].auth = AUTHENTICATION_Q3;
	memset (&args2, 0, sizeof (args2));
	/* FIXME: really do authentication */
	r = zfs_proc_auth_stage2_client_1 (t, &args2, fd);
	if (r != ZFS_OK)
	  goto node_authenticate_error;

	nod = node_lookup (sid);
	if (!nod)
	  {
	    r = ZFS_CONNECTION_CLOSED;
	    goto node_authenticate_error;
	  }
	if (!node_has_valid_fd (nod))
	  {
	    r = ZFS_CONNECTION_CLOSED;
	    goto node_authenticate_error;
	  }
	if (fd != nod->fd)
	  {
	    if (r >= ZFS_ERROR_HAS_DC_REPLY)
	      recycle_dc_to_fd_data (&t->dc_reply, &fd_data_a[nod->fd]);
	    zfsd_mutex_unlock (&nod->mutex);
	    goto again;
	  }

	/* FIXME: really do authentication */

	zfsd_mutex_unlock (&nod->mutex);
	fd_data_a[fd].auth = AUTHENTICATION_FINISHED;
	fd_data_a[fd].conn = CONNECTION_FAST;
	if (r >= ZFS_ERROR_HAS_DC_REPLY)
	  recycle_dc_to_fd_data (&t->dc_reply, &fd_data_a[fd]);
	zfsd_cond_broadcast (&fd_data_a[fd].cond);

	goto again;

      case AUTHENTICATION_Q3:
	while (fd_data_a[fd].auth == AUTHENTICATION_Q3)
	  {
	    zfsd_cond_wait (&fd_data_a[fd].cond,
			    &fd_data_a[fd].mutex);
	  }
	t->retval = ZFS_COULD_NOT_AUTH;
	goto again;

      case AUTHENTICATION_FINISHED:
	return fd;
    }

  return fd;

node_authenticate_error:
  t->retval = r;
  message (2, stderr, "not auth\n");
  zfsd_mutex_lock (&fd_data_a[fd].mutex);
  fd_data_a[fd].auth = AUTHENTICATION_NONE;
  fd_data_a[fd].conn = CONNECTION_NONE;
  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd_data (&t->dc_reply, &fd_data_a[fd]);
  close_network_fd (fd);
  zfsd_mutex_unlock (&fd_data_a[fd].mutex);
  if (nod)
    {
      nod->fd = -1;
      zfsd_mutex_unlock (&nod->mutex);
    }
  return false;
}

/* Check whether node NOD is connected and authenticated. If not do so.
   Return open file descriptor and leave its NETWORK_FD_DATA locked.  */

int
node_connect_and_authenticate (thread *t, node nod, authentication_status auth)
{
  int fd;

  CHECK_MUTEX_LOCKED (&nod->mutex);

  if (!node_has_valid_fd (nod))
    {
      time_t now;

      /* Do not try to connect too often.  */
      now = time (NULL);
      if (now - nod->last_connect < NODE_CONNECT_VISCOSITY)
	{
	  t->retval = ZFS_COULD_NOT_CONNECT;
	  zfsd_mutex_unlock (&nod->mutex);
	  return -1;
	}
      nod->last_connect = now;

      fd = node_connect (nod);
      if (fd < 0)
	{
	  t->retval = ZFS_COULD_NOT_CONNECT;
	  zfsd_mutex_unlock (&nod->mutex);
	  return -1;
	}
      add_fd_to_active (fd);
      update_node_fd (nod, fd, fd_data_a[fd].generation, true);
    }

  fd = node_authenticate (t, nod, auth);

  return fd;
}

/* Return true if current request came from this node.  */

bool
request_from_this_node ()
{
  thread *t;

  t = (thread *) pthread_getspecific (thread_data_key);
#ifdef ENABLE_CHECKING
  if (t == NULL)
    abort ();
#endif

  return t->from_sid == this_node->id;
}

/* Put DC back to file descriptor data FD_DATA.  */

void
recycle_dc_to_fd_data (DC *dc, fd_data_t *fd_data)
{
  CHECK_MUTEX_LOCKED (&fd_data->mutex);

  if (fd_data->fd >= 0 && fd_data->ndc < MAX_FREE_BUFFERS_PER_ACTIVE_FD)
    {
      /* Add the buffer to the queue.  */
      fd_data->dc[fd_data->ndc] = *dc;
      fd_data->ndc++;
    }
  else
    {
      /* Free the buffer.  */
      dc_destroy (dc);
    }
}

/* Put DC back to data for socket connected to master of volume VOL.  */

void
recycle_dc_to_fd (DC *dc, int fd)
{
  if (fd < 0)
    dc_destroy (dc);
  else
    {
      zfsd_mutex_lock (&fd_data_a[fd].mutex);
      recycle_dc_to_fd_data (dc, &fd_data_a[fd]);
      zfsd_mutex_unlock (&fd_data_a[fd].mutex);
    }
}

/* Helper function for sending request.  Send request with request id REQUEST_ID
   using data in thread T to connected socket FD and wait for reply.
   It expects fd_data_a[fd].mutex to be locked.  */

void
send_request (thread *t, uint32_t request_id, int fd)
{
  void **slot;
  waiting4reply_data *wd;

  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);

  if (thread_pool_terminate_p (&network_pool))
    {
      t->retval = ZFS_EXITING;
      zfsd_mutex_unlock (&fd_data_a[fd].mutex);
      return;
    }

  t->retval = ZFS_OK;

  /* Add the tread to the table of waiting threads.  */
  wd = ((waiting4reply_data *)
	pool_alloc (fd_data_a[fd].waiting4reply_pool));
  wd->request_id = request_id;
  wd->t = t;
  slot = htab_find_slot_with_hash (fd_data_a[fd].waiting4reply,
				   &request_id,
				   WAITING4REPLY_HASH (request_id), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = wd;
  wd->node = fibheap_insert (fd_data_a[fd].waiting4reply_heap,
			     (fibheapkey_t) time (NULL), wd);

  /* Send the request.  */
  fd_data_a[fd].last_use = time (NULL);
  if (!full_write (fd, t->dc_call.buffer, t->dc_call.cur_length))
    {
      t->retval = ZFS_CONNECTION_CLOSED;
      htab_clear_slot (fd_data_a[fd].waiting4reply, slot);
      fibheap_delete_node (fd_data_a[fd].waiting4reply_heap, wd->node);
      pool_free (fd_data_a[fd].waiting4reply_pool, wd);
      zfsd_mutex_unlock (&fd_data_a[fd].mutex);
      return;
    }
  zfsd_mutex_unlock (&fd_data_a[fd].mutex);

  /* Wait for reply.  */
  semaphore_down (&t->sem, 1);

  /* If there was no error with connection, decode return value.  */
  if (t->retval == ZFS_OK)
    {
      if (!decode_status (&t->dc_reply, &t->retval))
	t->retval = ZFS_INVALID_REPLY;
    }
}

/* Send a reply.  */

static void
send_reply (thread *t)
{
  network_thread_data *td = &t->u.network;

  message (2, stderr, "sending reply\n");
  zfsd_mutex_lock (&td->fd_data->mutex);

  /* Send a reply if we have not closed the file descriptor
     and we have not reopened it.  */
  if (td->fd_data->fd >= 0 && td->fd_data->generation == td->generation)
    {
      td->fd_data->last_use = time (NULL);
      if (!full_write (td->fd_data->fd, t->u.network.dc.buffer,
		       t->u.network.dc.cur_length))
	{
	}
    }
  zfsd_mutex_unlock (&td->fd_data->mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (thread *t, uint32_t request_id, int32_t status)
{
  start_encoding (&t->u.network.dc);
  encode_direction (&t->u.network.dc, DIR_REPLY);
  encode_request_id (&t->u.network.dc, request_id);
  encode_status (&t->u.network.dc, status);
  finish_encoding (&t->u.network.dc);
  send_reply (t);
}

/* Initialize network thread T.  */

void
network_worker_init (thread *t)
{
  dc_create (&t->dc_call, ZFS_MAX_REQUEST_LEN);
}

/* Cleanup network thread DATA.  */

void
network_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (&t->dc_call);
}

/* The main function of the network thread.  */

static void *
network_worker (void *data)
{
  thread *t = (thread *) data;
  network_thread_data *td = &t->u.network;
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  uint32_t request_id;
  uint32_t fn;

  thread_disable_signals ();

  pthread_cleanup_push (network_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Network worker thread");
  set_lock_info (li);

  while (1)
    {
      /* Wait until network_dispatch wakes us up.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (get_thread_state (t) == THREAD_DYING)
	break;

      if (!decode_request_id (&t->u.network.dc, &request_id))
	{
	  /* TODO: log too short packet.  */
	  goto out;
	}

      if (t->u.network.dc.max_length > t->u.network.dc.size)
	{
	  send_error_reply (t, request_id, ZFS_REQUEST_TOO_LONG);
	  goto out;
	}

      if (!decode_function (&t->u.network.dc, &fn))
	{
	  send_error_reply (t, request_id, ZFS_INVALID_REQUEST);
	  goto out;
	}

      message (2, stderr, "REQUEST: ID=%u function=%u\n", request_id, fn);
      switch (fn)
	{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
	  case ZFS_PROC_##NAME:						\
	    if (td->fd_data->auth < AUTH)				\
	      {								\
		send_error_reply (t, request_id,			\
				  ZFS_INVALID_AUTH_LEVEL);		\
		goto out;						\
	      }								\
	    if (!decode_##ARGS (&t->u.network.dc,			\
				&t->u.network.args.FUNCTION)		\
		|| !finish_decoding (&t->u.network.dc))			\
	      {								\
		send_error_reply (t, request_id, ZFS_INVALID_REQUEST);	\
		goto out;						\
	      }								\
	    call_statistics[CALL_FROM_NETWORK][NUMBER]++;		\
	    start_encoding (&t->u.network.dc);				\
	    encode_direction (&t->u.network.dc, DIR_REPLY);		\
	    encode_request_id (&t->u.network.dc, request_id);		\
	    zfs_proc_##FUNCTION##_server (&t->u.network.args.FUNCTION,	\
					  &t->u.network.dc,		\
					  &t->u.network, false);	\
	    finish_encoding (&t->u.network.dc);				\
	    send_reply (t);						\
	    break;
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

	  default:
	    send_error_reply (t, request_id, ZFS_UNKNOWN_FUNCTION);
	    goto out;
	}

out:
      zfsd_mutex_lock (&td->fd_data->mutex);
      td->fd_data->busy--;
      recycle_dc_to_fd_data (&t->u.network.dc, td->fd_data);
      zfsd_mutex_unlock (&td->fd_data->mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&network_pool.idle.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
	{
	  queue_put (&network_pool.idle, &t->index);
	  set_thread_state (t, THREAD_IDLE);
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (get_thread_state (t) != THREAD_DYING)
	    abort ();
#endif
	  zfsd_mutex_unlock (&network_pool.idle.mutex);
	  break;
	}
      zfsd_mutex_unlock (&network_pool.idle.mutex);
    }

  pthread_cleanup_pop (1);

  return NULL;
}

/* Function which gets a request and passes it to some network thread.
   It also regulates the number of network threads.  */

static bool
network_dispatch (fd_data_t *fd_data)
{
  DC *dc = &fd_data->dc[0];
  size_t index;
  direction dir;

  CHECK_MUTEX_LOCKED (&fd_data->mutex);

  if (verbose >= 3)
    print_dc (dc, stderr);

#ifdef ENABLE_CHECKING
  if (dc->cur_length != sizeof (uint32_t))
    abort ();
#endif

  if (!decode_direction (dc, &dir))
    {
      /* Invalid direction or packet too short, FIXME: log it.  */
      return false;
    }

  switch (dir)
    {
      case DIR_REPLY:
	/* Dispatch reply.  */

	if (1)
	  {
	    uint32_t request_id;
	    void **slot;
	    waiting4reply_data *data;
	    thread *t;

	    if (!decode_request_id (dc, &request_id))
	      {
		/* TODO: log too short packet.  */
		return false;
	      }
	    message (2, stderr, "REPLY: ID=%u\n", request_id);
	    slot = htab_find_slot_with_hash (fd_data->waiting4reply,
					     &request_id,
					     WAITING4REPLY_HASH (request_id),
					     NO_INSERT);
	    if (!slot)
	      {
		/* TODO: log request was not found.  */
		message (1, stderr, "Request ID %d has not been found.\n",
			 request_id);
		return false;
	      }

	    data = *(waiting4reply_data **) slot;
	    t = data->t;
	    t->dc_reply = *dc;
	    t->from_sid = fd_data->sid;
	    htab_clear_slot (fd_data->waiting4reply, slot);
	    fibheap_delete_node (fd_data->waiting4reply_heap, data->node);
	    pool_free (fd_data->waiting4reply_pool, data);

	    /* Let the thread run again.  */
	    semaphore_up (&t->sem, 1);
	  }
	break;

      case DIR_REQUEST:
	/* Dispatch request.  */
	zfsd_mutex_lock (&network_pool.idle.mutex);

	/* Regulate the number of threads.  */
	if (network_pool.idle.nelem == 0)
	  thread_pool_regulate (&network_pool);

	/* Select an idle thread and forward the request to it.  */
	queue_get (&network_pool.idle, &index);
#ifdef ENABLE_CHECKING
	if (get_thread_state (&network_pool.threads[index].t) == THREAD_BUSY)
	  abort ();
#endif
	set_thread_state (&network_pool.threads[index].t, THREAD_BUSY);
	network_pool.threads[index].t.from_sid = fd_data->sid;
	network_pool.threads[index].t.u.network.dc = *dc;
	network_pool.threads[index].t.u.network.fd_data = fd_data;
	network_pool.threads[index].t.u.network.generation
	  = fd_data->generation;

	/* Let the thread run.  */
	semaphore_up (&network_pool.threads[index].t.sem, 1);

	zfsd_mutex_unlock (&network_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */
	abort ();
    }

  return true;
}

/* Main function of the main (i.e. listening) network thread.  */

static void *
network_main (ATTRIBUTE_UNUSED void *data)
{
  struct pollfd *pfd;
  int i, n;
  ssize_t r;
  int accept_connections;
  time_t now;
  static char dummy[ZFS_MAXDATA];

  thread_disable_signals ();
  pthread_setspecific (thread_name_key, "Network main thread");

  pfd = (struct pollfd *) xmalloc (max_nfd * sizeof (struct pollfd));
  accept_connections = 1;

  while (!thread_pool_terminate_p (&network_pool))
    {
      fibheapkey_t threshold = (fibheapkey_t) time (NULL) - REQUEST_TIMEOUT;

      zfsd_mutex_lock (&active_mutex);
      for (i = 0; i < nactive; i++)
	{
	  zfsd_mutex_lock (&active[i]->mutex);
	  /* Timeout requests.  */
	  while (fibheap_min_key (active[i]->waiting4reply_heap) < threshold)
	    {
	      waiting4reply_data *data;
	      void **slot;

	      data = ((waiting4reply_data *)
		      fibheap_extract_min (active[i]->waiting4reply_heap));
	      slot = htab_find_slot_with_hash (active[i]->waiting4reply,
					       &data->request_id,
					       WAITING4REPLY_HASH (data->request_id),
					       NO_INSERT);
#ifdef ENABLE_CHECKING
	      if (!slot || !*slot)
		abort ();
#endif
	      data->t->retval = ZFS_REQUEST_TIMEOUT;
	      semaphore_up (&data->t->sem, 1);
	      htab_clear_slot (active[i]->waiting4reply, slot);
	      pool_free (active[i]->waiting4reply_pool, data);
	    }

#ifdef ENABLE_CHECKING
	  if (active[i]->conn == CONNECTION_NONE)
	    abort ();
#endif
	  pfd[i].fd = active[i]->fd;
	  pfd[i].events = (active[i]->conn == CONNECTION_CONNECTING
			   ? CAN_WRITE : CAN_READ);
	  zfsd_mutex_unlock (&active[i]->mutex);
	}
      if (accept_connections)
	{
	  pfd[nactive].fd = main_socket;
	  pfd[nactive].events = CAN_READ;
	}
      n = nactive;

      message (2, stderr, "Polling %d sockets\n", n + accept_connections);
      zfsd_mutex_lock (&network_pool.main_in_syscall);
      zfsd_mutex_unlock (&active_mutex);
      r = poll (pfd, n + accept_connections, 1000000);
      zfsd_mutex_unlock (&network_pool.main_in_syscall);
      message (2, stderr, "Poll returned %d, errno=%d\n", r, errno);

      if (thread_pool_terminate_p (&network_pool))
	{
	  message (2, stderr, "Terminating\n");
	  break;
	}

      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, network_main exiting\n", strerror (errno));
	  break;
	}

      if (r <= 0)
	continue;

      now = time (NULL);

      /* Decrease the number of (unprocessed) sockets with events
	 if there were events on main socket.  */
      if (pfd[n].revents)
	r--;

      zfsd_mutex_lock (&active_mutex);
      for (i = nactive - 1; i >= 0 && r > 0; i--)
	{
	  fd_data_t *fd_data = &fd_data_a[pfd[i].fd];

#ifdef ENABLE_CHECKING
	  if (pfd[i].fd < 0)
	    abort ();
#endif

	  message (2, stderr, "FD %d revents %d\n", pfd[i].fd, pfd[i].revents);
	  if ((pfd[i].revents & CANNOT_RW)
	      || ((fd_data->flags & FD_FLAG_CLOSE)
		  && fd_data->busy == 0
		  && fd_data->read == 0))
	    {
	      zfsd_mutex_lock (&fd_data->mutex);
	      close_active_fd (i);
	      zfsd_mutex_unlock (&fd_data->mutex);
	    }
	  else if (fd_data->conn == CONNECTION_CONNECTING)
	    {
	      int e;
	      socklen_t l = sizeof (e);

	      if (getsockopt (pfd[i].fd, SOL_SOCKET, SO_ERROR, &e, &l) < 0)
		{
		  message (2, stderr, "error on socket %d: %s\n", pfd[i].fd,
			   strerror (errno));
		  zfsd_mutex_lock (&fd_data->mutex);
		  close_active_fd (i);
		  zfsd_mutex_unlock (&fd_data->mutex);
		}
#ifdef ENABLE_CHECKING
	      else if (l != sizeof (e))
		abort ();
#endif
	      else if (e != 0)
		{
		  message (2, stderr, "error on socket %d: %s\n", pfd[i].fd,
			   strerror (e));
		  zfsd_mutex_lock (&fd_data->mutex);
		  close_active_fd (i);
		  zfsd_mutex_unlock (&fd_data->mutex);
		}

	      zfsd_mutex_lock (&fd_data->mutex);
	      fd_data->conn = CONNECTION_ACTIVE;
	      zfsd_cond_broadcast (&fd_data->cond);
	      zfsd_mutex_unlock (&fd_data->mutex);
/*	      message (2, stderr, "FD %d connected to %s\n", pfd[i].fd,
		       nod->name);*/
	    }
	  else if (pfd[i].revents & CAN_READ)
	    {
	      fd_data->last_use = now;
	      if (fd_data->read < 4)
		{
		  ssize_t r;

		  zfsd_mutex_lock (&fd_data->mutex);
		  if (fd_data->ndc == 0)
		    {
		      dc_create (&fd_data->dc[0], ZFS_MAX_REQUEST_LEN);
		      fd_data->ndc++;
		    }
		  zfsd_mutex_unlock (&fd_data->mutex);

		  r = read (fd_data->fd, fd_data->dc[0].buffer + fd_data->read,
			    4 - fd_data->read);
		  if (r <= 0)
		    {
		      zfsd_mutex_lock (&fd_data->mutex);
		      close_active_fd (i);
		      zfsd_mutex_unlock (&fd_data->mutex);
		    }
		  else
		    fd_data->read += r;

		  if (fd_data->read == 4)
		    {
		      start_decoding (&fd_data->dc[0]);
		    }
		}
	      else
		{
		  if (fd_data->dc[0].max_length <= fd_data->dc[0].size)
		    {
		      r = read (fd_data->fd,
				fd_data->dc[0].buffer + fd_data->read,
				fd_data->dc[0].max_length - fd_data->read);
		    }
		  else
		    {
		      int l;

		      l = fd_data->dc[0].max_length - fd_data->read;
		      if (l > ZFS_MAXDATA)
			l = ZFS_MAXDATA;
		      r = read (fd_data->fd, dummy, l);
		    }

		  if (r <= 0)
		    {
		      zfsd_mutex_lock (&fd_data->mutex);
		      close_active_fd (i);
		      zfsd_mutex_unlock (&fd_data->mutex);
		    }
		  else
		    {
		      fd_data->read += r;

		      if (fd_data->dc[0].max_length == fd_data->read)
			{
			  if (fd_data->dc[0].max_length <= fd_data->dc[0].size)
			    {
			      /* We have read complete request, dispatch it.  */
			      zfsd_mutex_lock (&fd_data->mutex);
			      fd_data->read = 0;
			      if (network_dispatch (fd_data))
				{
				  fd_data->busy++;
				  fd_data->ndc--;
				  if (fd_data->ndc > 0)
				    fd_data->dc[0] = fd_data->dc[fd_data->ndc];
				}
			      zfsd_mutex_unlock (&fd_data->mutex);
			    }
			  else
			    {
			      message (2, stderr, "Packet too long: %u\n",
				       fd_data->read);
			      fd_data->read = 0;
			    }
			}
		    }
		}
	    }

	  /* Decrease the number of (unprocessed) sockets with events.  */
	  if (pfd[i].revents)
	    r--;
	}

      if (accept_connections)
	{
	  if (pfd[n].revents & CANNOT_RW)
	    {
	      close (main_socket);
	      accept_connections = 0;
	      message (-1, stderr, "error on listening socket\n");
	    }
	  else if (pfd[n].revents & CAN_READ)
	    {
	      int s;
	      struct sockaddr_in ca;
	      socklen_t ca_len = sizeof (ca);

retry_accept:
	      s = accept (main_socket, (struct sockaddr *) &ca, &ca_len);

	      if ((s < 0 && errno == EMFILE)
		  || (s >= 0 && nactive >= max_network_sockets))
		{
		  time_t oldest = 0;
		  int index = -1;

		  /* Find the file descriptor which was unused for the longest
		     time.  */
		  for (i = 0; i < nactive; i++)
		    if (active[i]->busy == 0
			&& (active[i]->last_use < oldest || index < 0))
		      {
			index = i;
			oldest = active[i]->last_use;
		      }

		  if (index == -1)
		    {
		      /* All file descriptors are busy so close the new one.  */
		      message (2, stderr, "All filedescriptors are busy.\n");
		      if (s >= 0)
			close (s);
		      zfsd_mutex_unlock (&active_mutex);
		      continue;
		    }
		  else
		    {
		      fd_data_t *fd_data = active[index];

		      /* Close file descriptor unused for the longest time.  */
		      zfsd_mutex_lock (&fd_data->mutex);
		      close_active_fd (index);
		      zfsd_mutex_unlock (&fd_data->mutex);
		      goto retry_accept;
		    }
		}

	      if (s < 0)
		{
		  if (errno != EMFILE)
		    {
		      close (main_socket);
		      accept_connections = 0;
		      message (-1, stderr, "accept(): %s\n", strerror (errno));
		    }
		}
	      else
		{
		  message (2, stderr, "accepted FD %d\n", s);
		  zfsd_mutex_lock (&fd_data_a[s].mutex);
		  init_fd_data (s);
		  fd_data_a[s].conn = CONNECTION_PASSIVE;
		  zfsd_cond_broadcast (&fd_data_a[s].cond);
		  zfsd_mutex_unlock (&fd_data_a[s].mutex);
		}
	    }
	}
      zfsd_mutex_unlock (&active_mutex);
    }

  close (main_socket);

  /* Close idle file descriptors and free their memory.  */
  zfsd_mutex_lock (&active_mutex);
  for (i = nactive - 1; i >= 0; i--)
    {
      fd_data_t *fd_data = active[i];

      zfsd_mutex_lock (&fd_data->mutex);
      close_active_fd (i);
      zfsd_mutex_unlock (&fd_data->mutex);
    }
  zfsd_mutex_unlock (&active_mutex);
  free (pfd);
  message (2, stderr, "Terminating...\n");
  return NULL;
}

/* Initialize information about file descriptors.  */

void
fd_data_init ()
{
  int i;

  zfsd_mutex_init (&active_mutex);
  fd_data_a = (fd_data_t *) xcalloc (max_nfd, sizeof (fd_data_t));
  for (i = 0; i < max_nfd; i++)
    {
      zfsd_mutex_init (&fd_data_a[i].mutex);
      zfsd_cond_init (&fd_data_a[i].cond);
      fd_data_a[i].fd = -1;
    }

  nactive = 0;
  active = (fd_data_t **) xmalloc (max_nfd * sizeof (fd_data_t));
}

/* Wake threads waiting for reply on file descriptors.  */

void
fd_data_shutdown ()
{
  int i;

  /* Tell each thread waiting for reply that we are exiting.  */
  zfsd_mutex_lock (&active_mutex);
  i = nactive - 1;
  zfsd_mutex_unlock (&active_mutex);
  for (; i >= 0; i--)
    {
      fd_data_t *fd_data = active[i];

      zfsd_mutex_lock (&fd_data->mutex);
      wake_all_threads (fd_data, ZFS_EXITING);
      zfsd_mutex_unlock (&fd_data->mutex);
    }

  if (kernel_fd >= 0)
    {
      zfsd_mutex_lock (&fd_data_a[kernel_fd].mutex);
      wake_all_threads (fd_data_a, ZFS_EXITING);
      zfsd_mutex_unlock (&fd_data_a[kernel_fd].mutex);
    }
}

/* Destroy information about file descriptors.  */

void
fd_data_destroy ()
{
  int i;

  /* Close connected sockets.  */
  zfsd_mutex_lock (&active_mutex);
  for (i = nactive - 1; i >= 0; i--)
    {
      fd_data_t *fd_data = active[i];
      zfsd_mutex_lock (&fd_data->mutex);
      close_active_fd (i);
      zfsd_mutex_unlock (&fd_data->mutex);
    }
  zfsd_mutex_unlock (&active_mutex);
  zfsd_mutex_destroy (&active_mutex);

  close_kernel_fd ();

  for (i = 0; i < max_nfd; i++)
    {
      zfsd_mutex_destroy (&fd_data_a[i].mutex);
      zfsd_cond_destroy (&fd_data_a[i].cond);
    }

  free (active);
  free (fd_data_a);
}

/* Create a listening socket and start the main network thread.  */

bool
network_start ()
{
  socklen_t socket_options;
  struct sockaddr_in sa;

  /* Create a listening socket.  */
  main_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (main_socket < 0)
    {
      message (-1, stderr, "socket(): %s\n", strerror (errno));
      return false;
    }

  /* Reuse the port.  */
  socket_options = 1;
  if (setsockopt (main_socket, SOL_SOCKET, SO_REUSEADDR, &socket_options,
	      sizeof (socket_options)) != 0)
    {
      message (-1, stderr, "setsockopt(): %s\n", strerror (errno));
      close (main_socket);
      return false;
    }

  /* Bind the socket to ZFS_PORT.  */
  sa.sin_family = AF_INET;
  sa.sin_port = htons (ZFS_PORT);
  sa.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (main_socket, (struct sockaddr *) &sa, sizeof (sa)))
    {
      message (-1, stderr, "bind(): %s\n", strerror (errno));
      close (main_socket);
      return false;
    }

  /* Set the queue for incoming connections.  */
  if (listen (main_socket, SOMAXCONN) != 0)
    {
      message (-1, stderr, "listen(): %s\n", strerror (errno));
      close (main_socket);
      return false;
    }

  fd_data_init ();

  if (!thread_pool_create (&network_pool, 256, 4, 16, network_main,
			   network_worker, network_worker_init))
    {
      close (main_socket);
      fd_data_destroy ();
      return false;
    }

  return true;
}

/* Terminate network threads and destroy data structures.  */

void
network_cleanup ()
{
  thread_pool_destroy (&network_pool);
}
