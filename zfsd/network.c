/* Network thread functions.
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
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "semaphore.h"
#include "data-coding.h"
#include "network.h"
#include "log.h"
#include "util.h"
#include "malloc.h"
#include "thread.h"
#include "zfs_prot.h"
#include "node.h"
#include "volume.h"
#include "hashtab.h"
#include "alloc-pool.h"

/* Pool of network threads.  */
static thread_pool network_pool;

/* Data for network pool regulator.  */
thread_pool_regulator_data network_regulator_data;

/* Thread ID of the main network thread (thread receiving data from sockets).  */
pthread_t main_network_thread;

/* This mutex is locked when main network thread is in poll.  */
pthread_mutex_t main_network_thread_in_syscall;

/* File descriptor of the main (i.e. listening) socket.  */
static int main_socket;

/* The array of data for each file descriptor.  */
network_fd_data_t *network_fd_data;

/* Array of pointers to data of active file descriptors.  */
static network_fd_data_t **active;

/* Number of active file descriptors.  */
static int nactive;

/* Mutex protecting access to ACTIVE and NACTIVE.  */
static pthread_mutex_t active_mutex;

/* Hash function for request ID.  */
#define WAITING4REPLY_HASH(REQUEST_ID) (REQUEST_ID)

/* Hash function for waiting4reply_data.  */

static hash_t
waiting4reply_hash (const void *xx)
{
  const waiting4reply_data *x = (waiting4reply_data *) xx;

  return WAITING4REPLY_HASH (x->request_id);
}

/* Return true when waiting4reply_data XX is data for request ID *YY.  */

static int
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
  CHECK_MUTEX_LOCKED (&network_fd_data[fd].mutex);

  /* Set the network file descriptor's data.  */
  active[nactive] = &network_fd_data[fd];
  nactive++;
  network_fd_data[fd].fd = fd;
  network_fd_data[fd].read = 0;
  if (network_fd_data[fd].ndc == 0)
    {
      dc_create (&network_fd_data[fd].dc[0], ZFS_MAX_REQUEST_LEN);
      network_fd_data[fd].ndc++;
    }
  network_fd_data[fd].last_use = time (NULL);
  network_fd_data[fd].generation++;
  network_fd_data[fd].busy = 0;
  network_fd_data[fd].flags = 0;

  network_fd_data[fd].waiting4reply_pool
    = create_alloc_pool ("waiting4reply_data",
			 sizeof (waiting4reply_data), 30,
			 &network_fd_data[fd].mutex);
  network_fd_data[fd].waiting4reply_heap
    = fibheap_new (30, &network_fd_data[fd].mutex);
  network_fd_data[fd].waiting4reply
    = htab_create (30, waiting4reply_hash, waiting4reply_eq,
		   NULL, &network_fd_data[fd].mutex);
}

/* Close file descriptor FD and update its network_fd_data.  */

void
close_network_fd (int fd)
{
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&network_fd_data[fd].mutex);

  message (2, stderr, "Closing FD %d\n", fd);
  close (fd);
  network_fd_data[fd].generation++;
  network_fd_data[fd].auth = AUTHENTICATION_NONE;
}

/* Close an active file descriptor on index I in ACTIVE.  */

static void
close_active_fd (int i)
{
  int fd = active[i]->fd;
  int j;
  void **slot;

  CHECK_MUTEX_LOCKED (&active_mutex);
  CHECK_MUTEX_LOCKED (&network_fd_data[fd].mutex);

  close_network_fd (fd);
  nactive--;
  if (i < nactive)
    active[i] = active[nactive];
  for (j = 0; j < network_fd_data[fd].ndc; j++)
    dc_destroy (&network_fd_data[fd].dc[j]);
  network_fd_data[fd].ndc = 0;
  network_fd_data[fd].fd = -1;
  HTAB_FOR_EACH_SLOT (network_fd_data[fd].waiting4reply, slot,
    {
      waiting4reply_data *data = *(waiting4reply_data **) slot;

      data->t->retval = ZFS_CONNECTION_CLOSED;
      semaphore_up (&data->t->sem, 1);
    });
  htab_destroy (network_fd_data[fd].waiting4reply);
  fibheap_delete (network_fd_data[fd].waiting4reply_heap);
  free_alloc_pool (network_fd_data[fd].waiting4reply_pool);
}

/* Put DC back to file descriptor data FD_DATA.  */

void
recycle_dc_to_fd_data (DC *dc, network_fd_data_t *fd_data)
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
      zfsd_mutex_lock (&network_fd_data[fd].mutex);
      recycle_dc_to_fd_data (dc, &network_fd_data[fd]);
      zfsd_mutex_unlock (&network_fd_data[fd].mutex);
    }
}

/* Helper function for sending request.  Send request with request id REQUEST_ID
   using data in thread T to connected socket FD and wait for reply.
   It expects network_fd_data[fd].mutex to be locked.  */

void
send_request (thread *t, uint32_t request_id, int fd)
{
  void **slot;
  waiting4reply_data *wd;

  CHECK_MUTEX_LOCKED (&network_fd_data[fd].mutex);

  if (!get_running ())
    {
      t->retval = ZFS_EXITING;
      zfsd_mutex_unlock (&network_fd_data[fd].mutex);
      return;
    }

  t->retval = ZFS_OK;

  /* Add the tread to the table of waiting threads.  */
  wd = ((waiting4reply_data *)
	pool_alloc (network_fd_data[fd].waiting4reply_pool));
  wd->request_id = request_id;
  wd->t = t;
  slot = htab_find_slot_with_hash (network_fd_data[fd].waiting4reply,
				   &request_id,
				   WAITING4REPLY_HASH (request_id), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = wd;
  wd->node = fibheap_insert (network_fd_data[fd].waiting4reply_heap,
			     (fibheapkey_t) time (NULL), wd);

  /* Send the request.  */
  network_fd_data[fd].last_use = time (NULL);
  if (!full_write (fd, t->dc_call.buffer, t->dc_call.cur_length))
    {
      t->retval = ZFS_CONNECTION_CLOSED;
      htab_clear_slot (network_fd_data[fd].waiting4reply, slot);
      fibheap_delete_node (network_fd_data[fd].waiting4reply_heap, wd->node);
      pool_free (network_fd_data[fd].waiting4reply_pool, wd);
      zfsd_mutex_unlock (&network_fd_data[fd].mutex);
      return;
    }
  zfsd_mutex_unlock (&network_fd_data[fd].mutex);

  /* Wait for reply.  */
  semaphore_down (&t->sem, 1);

  /* If there was no error with connection, decode return value.  */
  if (t->retval == ZFS_OK)
    {
      if (!decode_status (&t->dc_reply, &t->retval))
	t->retval = ZFS_INVALID_REPLY;
    }
}

/* Add file descriptor FD to the set of active file descriptors.  */

void
add_fd_to_active (int fd)
{
  zfsd_mutex_lock (&active_mutex);
  zfsd_mutex_lock (&network_fd_data[fd].mutex);
  init_fd_data (fd);
  thread_terminate_blocking_syscall (main_network_thread, &main_network_thread_in_syscall);
  zfsd_mutex_unlock (&active_mutex);
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
      if (!full_write (td->fd_data->fd, t->dc.buffer, t->dc.cur_length))
	{
	}
    }
  zfsd_mutex_unlock (&td->fd_data->mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (thread *t, uint32_t request_id, int32_t status)
{
  start_encoding (&t->dc);
  encode_direction (&t->dc, DIR_REPLY);
  encode_request_id (&t->dc, request_id);
  encode_status (&t->dc, status);
  finish_encoding (&t->dc);
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
  uint32_t request_id;
  uint32_t fn;

  thread_disable_signals ();

  pthread_cleanup_push (network_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);

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

      if (!decode_request_id (&t->dc, &request_id))
	{
	  /* TODO: log too short packet.  */
	  goto out;
	}

      if (t->dc.max_length > t->dc.size)
	{
	  send_error_reply (t, request_id, ZFS_REQUEST_TOO_LONG);
	  goto out;
	}

      if (!decode_function (&t->dc, &fn))
	{
	  send_error_reply (t, request_id, ZFS_INVALID_REQUEST);
	  goto out;
	}

      message (2, stderr, "REQUEST: ID=%u function=%u\n", request_id, fn);
      switch (fn)
	{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		      \
	  case ZFS_PROC_##NAME:						      \
	    if (td->fd_data->auth < AUTH)					      \
	      {								      \
		send_error_reply (t, request_id, ZFS_INVALID_AUTH_LEVEL);     \
		goto out;						      \
	      }								      \
	    if (!decode_##ARGS (&t->dc, &t->args.FUNCTION)		      \
		|| !finish_decoding (&t->dc))				      \
	      {								      \
		send_error_reply (t, request_id, ZFS_INVALID_REQUEST);	      \
		goto out;						      \
	      }								      \
	    start_encoding (&t->dc);					      \
	    encode_direction (&t->dc, DIR_REPLY);			      \
	    encode_request_id (&t->dc, request_id);			      \
	    zfs_proc_##FUNCTION##_server (&t->args.FUNCTION, t, false);	      \
	    finish_encoding (&t->dc);					      \
	    send_reply (t);						      \
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
      recycle_dc_to_fd_data (&t->dc, td->fd_data);
      zfsd_mutex_unlock (&td->fd_data->mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&network_pool.idle.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
	{
	  queue_put (&network_pool.idle, t->index);
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

  return data;
}

/* Function which gets a request and passes it to some network thread.
   It also regulates the number of network threads.  */

static void
network_dispatch (network_fd_data_t *fd_data, DC *dc, unsigned int generation)
{
  size_t index;
  direction dir;

  if (!decode_direction (dc, &dir))
    {
      /* Invalid direction or packet too short, FIXME: log it.  */
      return;
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
		break;
	      }
	    message (2, stderr, "REPLY: ID=%u\n", request_id);
	    zfsd_mutex_lock (&fd_data->mutex);
	    slot = htab_find_slot_with_hash (fd_data->waiting4reply,
					     &request_id,
					     WAITING4REPLY_HASH (request_id),
					     NO_INSERT);
	    if (!slot)
	      {
		recycle_dc_to_fd_data (dc, fd_data);
		zfsd_mutex_unlock (&fd_data->mutex);
		/* TODO: log request was not found.  */
		message (1, stderr, "Request ID %d has not been found.\n",
			 request_id);
		break;
	      }

	    data = *(waiting4reply_data **) slot;
	    t = data->t;
	    t->dc_reply = *dc;
	    htab_clear_slot (fd_data->waiting4reply, slot);
	    fibheap_delete_node (fd_data->waiting4reply_heap, data->node);
	    pool_free (fd_data->waiting4reply_pool, data);
	    zfsd_mutex_unlock (&fd_data->mutex);

	    /* Let the thread run again.  */
	    semaphore_up (&t->sem, 1);
	  }
	break;

      case DIR_REQUEST:
	/* Dispatch request.  */

	zfsd_mutex_lock (&network_pool.idle.mutex);

	/* Regulate the number of threads.  */
	thread_pool_regulate (&network_pool, network_worker, NULL);

	/* Select an idle thread and forward the request to it.  */
	index = queue_get (&network_pool.idle);
#ifdef ENABLE_CHECKING
	if (get_thread_state (&network_pool.threads[index].t) == THREAD_BUSY)
	  abort ();
#endif
	set_thread_state (&network_pool.threads[index].t, THREAD_BUSY);
	network_pool.threads[index].t.u.network.fd_data = fd_data;
	network_pool.threads[index].t.dc = *dc;
	network_pool.threads[index].t.u.network.generation = generation;

	/* Let the thread run.  */
	semaphore_up (&network_pool.threads[index].t.sem, 1);

	zfsd_mutex_unlock (&network_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */
	abort ();
    }
}

/* Create network threads and related threads.  */

bool
create_network_threads ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&network_pool, 256, 4, 16);

  zfsd_mutex_lock (&network_pool.idle.mutex);
  zfsd_mutex_lock (&network_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 5; i++)
    {
      create_idle_thread (&network_pool, network_worker, network_worker_init);
    }
  zfsd_mutex_unlock (&network_pool.empty.mutex);
  zfsd_mutex_unlock (&network_pool.idle.mutex);

  thread_pool_create_regulator (&network_regulator_data, &network_pool,
				network_worker, network_worker_init);
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

  pfd = (struct pollfd *) xmalloc (max_nfd * sizeof (struct pollfd));
  accept_connections = 1;

  while (get_running ())
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

	      data = fibheap_extract_min (active[i]->waiting4reply_heap);
	      slot = htab_find_slot_with_hash (active[i]->waiting4reply,
					       &data->request_id,
					       WAITING4REPLY_HASH (data->request_id),
					       NO_INSERT);
#ifdef ENABLE_CHECKING
	      if (!slot || !*slot)
		abort ();
#endif
	      data->t->retval = ZFS_REQUEST_TIMEOUT;
	      htab_clear_slot (active[i]->waiting4reply, slot);
	      pool_free (active[i]->waiting4reply_pool, data);
	      semaphore_up (&data->t->sem, 1);
	    }

	  pfd[i].fd = active[i]->fd;
	  pfd[i].events = CAN_READ;
	  zfsd_mutex_unlock (&active[i]->mutex);
	}
      if (accept_connections)
	{
	  pfd[nactive].fd = main_socket;
	  pfd[nactive].events = CAN_READ;
	}
      n = nactive;

      message (2, stderr, "Polling %d sockets\n", n + accept_connections);
      zfsd_mutex_lock (&main_network_thread_in_syscall);
      zfsd_mutex_unlock (&active_mutex);
      r = poll (pfd, n + accept_connections, 1000000);
      zfsd_mutex_unlock (&main_network_thread_in_syscall);
      message (2, stderr, "Poll returned %d, errno=%d\n", r, errno);

      if (!get_running ())
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
	  network_fd_data_t *fd_data = &network_fd_data[pfd[i].fd];

	  message (2, stderr, "FD %d revents %d\n", pfd[i].fd, pfd[i].revents);
	  if ((pfd[i].revents & CANNOT_RW)
	      || ((fd_data->flags & NETWORK_FD_CLOSE)
		  && fd_data->busy == 0
		  && fd_data->read == 0))
	    {
	      zfsd_mutex_lock (&fd_data->mutex);
	      close_active_fd (i);
	      zfsd_mutex_unlock (&fd_data->mutex);
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
			      unsigned int generation;
			      DC *dc;

			      zfsd_mutex_lock (&fd_data->mutex);
			      generation = fd_data->generation;
			      dc = &fd_data->dc[0];
			      fd_data->read = 0;
			      fd_data->busy++;
			      fd_data->ndc--;
			      if (fd_data->ndc > 0)
				fd_data->dc[0] = fd_data->dc[fd_data->ndc];
			      zfsd_mutex_unlock (&fd_data->mutex);

			      /* We have read complete request, dispatch it.  */
			      network_dispatch (fd_data, dc, generation);
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
		      network_fd_data_t *fd_data = active[index];

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
		  zfsd_mutex_lock (&network_fd_data[s].mutex);
		  init_fd_data (s);
		  zfsd_mutex_unlock (&network_fd_data[s].mutex);
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
      network_fd_data_t *fd_data = active[i];

      zfsd_mutex_lock (&fd_data->mutex);
      close_active_fd (i);
      zfsd_mutex_unlock (&fd_data->mutex);
    }
  zfsd_mutex_unlock (&active_mutex);
  free (pfd);
  message (2, stderr, "Terminating...\n");
  return NULL;
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

  /* Create the main network thread.  */
  if (pthread_create (&main_network_thread, NULL, network_main, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
      free (network_fd_data);
      close (main_socket);
      return false;
    }

  return true;
}

/* Initialize information about network file descriptors.  */

bool
init_network_fd_data ()
{
  int i;

  zfsd_mutex_init (&active_mutex);
  network_fd_data = (network_fd_data_t *) xcalloc (max_nfd,
						 sizeof (network_fd_data_t));
  for (i = 0; i < max_nfd; i++)
    {
      zfsd_mutex_init (&network_fd_data[i].mutex);
      network_fd_data[i].fd = -1;
    }

  nactive = 0;
  active = (network_fd_data_t **) xmalloc (max_nfd * sizeof (network_fd_data_t));

  return true;
}

/* Destroy information about network file descriptors.  */

void
network_destroy_fd_data ()
{
  int i;

  /* Close connected sockets.  */
  zfsd_mutex_lock (&active_mutex);
  for (i = nactive - 1; i >= 0; i--)
    {
      network_fd_data_t *fd_data = active[i];
      zfsd_mutex_lock (&fd_data->mutex);
      close_active_fd (i);
      zfsd_mutex_unlock (&fd_data->mutex);
    }
  zfsd_mutex_unlock (&active_mutex);
  zfsd_mutex_destroy (&active_mutex);

  for (i = 0; i < max_nfd; i++)
    zfsd_mutex_destroy (&network_fd_data[i].mutex);

  free (active);
  free (network_fd_data);
}

/* Terminate network threads and destroy data structures.  */

void
network_cleanup ()
{
  int i;

  /* Tell each thread waiting for reply that we are exiting.  */
  zfsd_mutex_lock (&active_mutex);
  i = nactive - 1;
  zfsd_mutex_unlock (&active_mutex);
  for (; i >= 0; i--)
    {
      network_fd_data_t *fd_data = active[i];
      void **slot;

      zfsd_mutex_lock (&fd_data->mutex);
      HTAB_FOR_EACH_SLOT (fd_data->waiting4reply, slot,
	{
	  waiting4reply_data *data = *(waiting4reply_data **) slot;

	  data->t->retval = ZFS_EXITING;
	  htab_clear_slot (fd_data->waiting4reply, slot);
	  fibheap_delete_node (fd_data->waiting4reply_heap, data->node);
	  pool_free (fd_data->waiting4reply_pool, data);
	  semaphore_up (&data->t->sem, 1);
	});
      zfsd_mutex_unlock (&fd_data->mutex);
    }

  thread_pool_destroy (&network_pool);
  network_destroy_fd_data ();
}
