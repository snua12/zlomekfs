/* Server thread functions.
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
#include "data-coding.h"
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include "constant.h"
#include "semaphore.h"
#include "server.h"
#include "log.h"
#include "malloc.h"
#include "thread.h"
#include "zfs_prot.h"

/* Pool of server threads.  */
static thread_pool server_pool;

/* Data for server pool regulator.  */
static thread_pool_regulator_data server_regulator_data;

#ifdef RPC
#if 0

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

extern void zfs_program_1 (struct svc_req *rqstp, SVCXPRT *transp);

/* Local function prototypes.  */
static void *server_worker (void *data);
static void server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp);

/* Function which receives a RPC request and passes it to some server thread.
   It also regulates the number of server threads.  */

static void
server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp)
{
  size_t index;

  pthread_mutex_lock (&server_pool.idle.mutex);

  /* Regulate the number of threads.  */
  thread_pool_regulate (&server_pool, server_worker, NULL);

  /* Select an idle thread and forward the request to it.  */
  index = queue_get (&server_pool.idle);
#ifdef ENABLE_CHECKING
  if (server_pool.threads[index].t.state == THREAD_BUSY)
    abort ();
#endif
  server_pool.threads[index].t.state = THREAD_BUSY;
  server_pool.threads[index].t.u.server.rqstp = rqstp;
  server_pool.threads[index].t.u.server.transp = transp;
  printf ("%p %p\n", rqstp, transp);
  pthread_mutex_unlock (&server_pool.threads[index].t.mutex);

  pthread_mutex_unlock (&server_pool.idle.mutex);
}

/* The main function of the server thread.  */

static void *
server_worker (void *data)
{
  thread *t = (thread *) data;

  while (1)
    {
      /* Wait until server_dispatch wakes us up.  */
      pthread_mutex_lock (&t->mutex);

#ifdef ENABLE_CHECKING
      if (t->state == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (t->state == THREAD_DYING)
	return data;

#ifdef RPC
      /* We have some work to do.  */
      zfs_program_1 (t->u.server.rqstp, t->u.server.transp);
#endif

      /* Put self to the idle queue if not requested to die meanwhile.  */
      pthread_mutex_lock (&server_pool.idle.mutex);
      if (t->state == THREAD_BUSY)
	{
	  queue_put (&server_pool.idle, t->index);
	  t->state = THREAD_IDLE;
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (t->state != THREAD_DYING)
	    abort ();
#endif
	  pthread_mutex_unlock (&server_pool.idle.mutex);
	  break;
	}
      pthread_mutex_unlock (&server_pool.idle.mutex);
    }

  return data;
}

#endif
#else	/* RPC */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "data-coding.h"
#include "memory.h"
#include "node.h"
#include "hashtab.h"
#include "alloc-pool.h"

/* Thread ID of the main server thread (thread receiving data from sockets).  */
pthread_t main_server_thread;

/* File descriptor of the main (i.e. listening) socket.  */
static int main_socket;

/* The array of data for each file descriptor.  */
server_fd_data_t *server_fd_data;

/* Array of pointers to data of active file descriptors.  */
static server_fd_data_t **active;

/* Number of active file descriptors.  */
static int nactive;

/* Mutex for accessing active and nactive.  */
static pthread_mutex_t active_mutex;

/* Hash function for request ID.  */
#define WAITING4REPLY_HASH(REQUEST_ID) (REQUEST_ID)

/* Hash function for waiting4reply_data.  */

static hash_t
waiting4reply_hash (const void *xx)
{
  const waiting4reply_data *x = (waiting4reply_data *) xx;

  return WAITING4REPLY_HASH(x->request_id);
}

/* Return true when waiting4reply_data XX is data for request ID *YY.  */

static int
waiting4reply_eq (const void *xx, const void *yy)
{
  const waiting4reply_data *x = (waiting4reply_data *) xx;
  const unsigned int id = *(unsigned int *) yy;

  return WAITING4REPLY_HASH(x->request_id) == id;
}

/* Initialize data for file descriptor FD and add it to ACTIVE.  */

static void
init_fd_data (int fd)
{
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&active_mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
  if (fd < 0)
    abort ();
#endif
  /* Set the server's data.  */
  active[nactive] = &server_fd_data[fd];
  nactive++;
  server_fd_data[fd].fd = fd;
  server_fd_data[fd].read = 0;
  if (server_fd_data[fd].ndc == 0)
    {
      dc_create (&server_fd_data[fd].dc[0], ZFS_MAX_REQUEST_LEN);
      server_fd_data[fd].ndc++;
    }
  server_fd_data[fd].last_use = time (NULL);
  server_fd_data[fd].generation++;
  server_fd_data[fd].busy = 0;
  server_fd_data[fd].flags = 0;

  server_fd_data[fd].waiting4reply_pool
    = create_alloc_pool ("waiting4reply_data",
			 sizeof (waiting4reply_data), 30,
			 &server_fd_data[fd].mutex);
  server_fd_data[fd].waiting4reply
    = htab_create (30, waiting4reply_hash, waiting4reply_eq,
		   NULL, &server_fd_data[fd].mutex);
}

/* Close file descriptor FD and update its server_fd_data.  */
void
close_server_fd (int fd)
{
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
  if (fd < 0)
    abort ();
#endif

  message (2, stderr, "Closing FD %d\n", fd);
  close (fd);
  server_fd_data[fd].generation++;
  server_fd_data[fd].auth = AUTHENTICATION_NONE;
}

/* Close an active file descriptor on index I in ACTIVE.  */

static void
close_active_fd (int i)
{
  int fd = active[i]->fd;
  int j;
  void **slot;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&active_mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
#endif
  close_server_fd (fd);
  active[i] = active[nactive];
  nactive--;
  for (j = 0; j < server_fd_data[fd].ndc; j++)
    dc_destroy (&server_fd_data[fd].dc[j]);
  server_fd_data[fd].ndc = 0;
  server_fd_data[fd].fd = -1;
  HTAB_FOR_EACH_SLOT (server_fd_data[fd].waiting4reply, slot,
    {
      waiting4reply_data *data = *(waiting4reply_data **) slot;

      data->t->u.server.retval = ZFS_CONNECTION_CLOSED;
      semaphore_up (&data->t->sem, 1);
    });
  htab_destroy (server_fd_data[fd].waiting4reply);
  free_alloc_pool (server_fd_data[fd].waiting4reply_pool);
}

/* Safely write LEN bytes to file descriptor FD data from buffer BUF.  */

static bool
safe_write (int fd, char *buf, size_t len)
{
  ssize_t w;
  unsigned int written;

  if (verbose >= 2)
    {
      size_t i;

      message (2, stderr, "Sending data from %p:\n", buf);
      for (i = 0; i < len; i++)
	fprintf (stderr, "%02x ", (unsigned char) buf[i]);
      fprintf (stderr, "\n");
    }

  for (written = 0; written < len; written += w)
    {
      w = write (fd, buf + written, len - written);
      if (w <= 0)
	return false;
    }

  return true;
}

/* Helper function for sending request.  Send request with request id REQUEST_ID
   using data in thread T to connected socket FD and wait for reply.
   It expects server_fd_data[fd].mutex to be locked.  */

void
send_request (thread *t, uint32_t request_id, int fd)
{
  server_thread_data *td = &t->u.server;
  void **slot;
  waiting4reply_data *wd;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
#endif

  if (!running)
    {
      td->retval = ZFS_EXITING;
      return;
    }
  
  /* Add the tread to the table of waiting threads.  */
  wd = ((waiting4reply_data *)
	pool_alloc (server_fd_data[fd].waiting4reply_pool));
  wd->request_id = request_id;
  wd->t = t;
#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (server_fd_data[fd].waiting4reply,
				   &request_id,
				   WAITING4REPLY_HASH (request_id), NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (server_fd_data[fd].waiting4reply,
				   &request_id,
				   WAITING4REPLY_HASH (request_id), INSERT);
  *slot = wd;

  /* Send the request.  */
  server_fd_data[fd].last_use = time (NULL);
  if (!safe_write (fd, td->dc_call.buffer, td->dc_call.cur_length))
    {
      pthread_mutex_unlock (&server_fd_data[fd].mutex);
      td->retval = ZFS_CONNECTION_CLOSED;
      htab_clear_slot (server_fd_data[fd].waiting4reply, slot);
      return;
    }
  pthread_mutex_unlock (&server_fd_data[fd].mutex);

  /* Wait for reply.  */
  semaphore_down (&t->sem, 1);
  
  /* Decode return value.  */
  if (!decode_status (&td->dc, &td->retval))
    td->retval = ZFS_INVALID_REPLY;
}

/* Add file descriptor FD to the set of active file descriptors.  */

void
add_fd_to_active (int fd)
{
  pthread_mutex_lock (&active_mutex);
  pthread_mutex_lock (&server_fd_data[fd].mutex);
  init_fd_data (fd);
  pthread_kill (main_server_thread, SIGUSR1);	/* terminate poll */
  pthread_mutex_unlock (&active_mutex);
}

/* Send a reply.  */

static void
send_reply (server_thread_data *td)
{
  message (2, stderr, "sending reply\n");
  pthread_mutex_lock (&td->fd_data->mutex);

  /* Send a reply if we have not closed the file descriptor
     and we have not reopened it.  */
  if (td->fd_data->fd >= 0 && td->fd_data->generation == td->generation)
    {
      td->fd_data->last_use = time (NULL);
      if (!safe_write (td->fd_data->fd, td->dc.buffer, td->dc.cur_length))
	{
	}
    }
  pthread_mutex_unlock (&td->fd_data->mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (server_thread_data *td, uint32_t request_id, int status)
{
  start_encoding (&td->dc);
  encode_direction (&td->dc, DIR_REPLY);
  encode_request_id (&td->dc, request_id);
  encode_status (&td->dc, status);
  finish_encoding (&td->dc);
  send_reply (td);
}

/* Initialize server thread T.  */

void
server_worker_init (thread *t)
{
  dc_create (&t->u.server.dc_call, ZFS_MAX_REQUEST_LEN);
}

/* Cleanup server thread DATA.  */

void
server_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (&t->u.server.dc_call);
}

/* The main function of the server thread.  */

static void *
server_worker (void *data)
{
  thread *t = (thread *) data;
  server_thread_data *td = &t->u.server;
  server_fd_data_t *fd_data;
  uint32_t request_id;
  uint32_t fn;

  pthread_cleanup_push (server_worker_cleanup, data);
  pthread_setspecific (server_thread_key, data);

  while (1)
    {
      /* Wait until server_dispatch wakes us up.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (t->state == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (t->state == THREAD_DYING)
	break;

      if (!decode_request_id (&td->dc, &request_id))
	{
	  /* TODO: log too short packet.  */
	  goto out;
	}

      if (td->dc.max_length > td->dc.size)
	{
	  send_error_reply (td, request_id, ZFS_REQUEST_TOO_LONG);
	  goto out;
	}

      if (!decode_function (&td->dc, &fn))
	{
	  send_error_reply (td, request_id, ZFS_INVALID_REQUEST);
	  goto out;
	}

      message (2, stderr, "REQUEST: ID=%u function=%u\n", request_id, fn);
      fd_data = td->fd_data;
      switch (fn)
	{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		      \
	  case ZFS_PROC_##NAME:						      \
	    if (fd_data->auth < AUTH)					      \
	      {								      \
		send_error_reply (td, request_id, ZFS_INVALID_AUTH_LEVEL);    \
		goto out;						      \
	      }								      \
	    if (!decode_##ARGS (&td->dc, &td->args.FUNCTION)		      \
		|| !finish_decoding (&td->dc))				      \
	      {								      \
		send_error_reply (td, request_id, ZFS_INVALID_REQUEST);	      \
		goto out;						      \
	      }								      \
	    start_encoding (&td->dc);					      \
	    encode_direction (&td->dc, DIR_REPLY);			      \
	    encode_request_id (&td->dc, request_id);			      \
	    zfs_proc_##FUNCTION##_server (&td->args.FUNCTION, t);	      \
	    finish_encoding (&td->dc);					      \
	    send_reply (td);						      \
	    break;
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
		
	  default:
	    send_error_reply (td, request_id, ZFS_UNKNOWN_FUNCTION);
	    goto out;
	}

out:
      pthread_mutex_lock (&fd_data->mutex);
      fd_data->busy--;
      if (running)
	{
	  if (fd_data->ndc < MAX_FREE_BUFFERS_PER_SERVER_FD)
	    {
	      /* Add the buffer to the queue.  */
	      fd_data->dc[fd_data->ndc] = td->dc;
	      fd_data->ndc++;
	    }
	  else
	    {
	      /* Free the buffer.  */
	      dc_destroy (&td->dc);
	    }
	}
      pthread_mutex_unlock (&fd_data->mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      pthread_mutex_lock (&server_pool.idle.mutex);
      if (t->state == THREAD_BUSY)
	{
	  queue_put (&server_pool.idle, t->index);
	  t->state = THREAD_IDLE;
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (t->state != THREAD_DYING)
	    abort ();
#endif
	  pthread_mutex_unlock (&server_pool.idle.mutex);
	  break;
	}
      pthread_mutex_unlock (&server_pool.idle.mutex);
    }

  pthread_cleanup_pop (1);

  return data;
}

/* Function which gets a request and passes it to some server thread.
   It also regulates the number of server threads.  */

static void
server_dispatch (server_fd_data_t *fd_data, DC *dc, unsigned int generation)
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
	    pthread_mutex_lock (&fd_data->mutex);
	    slot = htab_find_slot_with_hash (fd_data->waiting4reply,
					     &request_id,
					     WAITING4REPLY_HASH (request_id),
					     NO_INSERT);
	    if (!slot)
	      {
		pthread_mutex_unlock (&fd_data->mutex);
		/* TODO: log request was not found.  */
		message (1, stderr, "Request ID %d has not been found.\n",
			 request_id);
		break;
	      }

	    data = *(waiting4reply_data **) slot;
	    t = data->t;
	    t->u.server.dc = *dc;
	    htab_clear_slot (fd_data->waiting4reply, slot);
	    pool_free (fd_data->waiting4reply_pool, data);
	    pthread_mutex_unlock (&fd_data->mutex);

	    /* Let the thread run again.  */
	    semaphore_up (&t->sem, 1);
	  }
	break;

      case DIR_REQUEST:
	/* Dispatch request.  */

	pthread_mutex_lock (&server_pool.idle.mutex);

	/* Regulate the number of threads.  */
	thread_pool_regulate (&server_pool, server_worker, NULL);

	/* Select an idle thread and forward the request to it.  */
	index = queue_get (&server_pool.idle);
#ifdef ENABLE_CHECKING
	if (server_pool.threads[index].t.state == THREAD_BUSY)
	  abort ();
#endif
	server_pool.threads[index].t.state = THREAD_BUSY;
	server_pool.threads[index].t.u.server.fd_data = fd_data;
	server_pool.threads[index].t.u.server.dc = *dc;
	server_pool.threads[index].t.u.server.generation = generation;

	/* Let the thread run.  */
	semaphore_up (&server_pool.threads[index].t.sem, 1);

	pthread_mutex_unlock (&server_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */ 
	abort ();
    }
}

#endif

/* Create server threads and related threads.  */

bool
create_server_threads ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&server_pool, 256, 4, 16);

  pthread_mutex_lock (&server_pool.idle.mutex);
  pthread_mutex_lock (&server_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      create_idle_thread (&server_pool, server_worker, server_worker_init);
    }
  pthread_mutex_unlock (&server_pool.empty.mutex);
  pthread_mutex_unlock (&server_pool.idle.mutex);

  thread_pool_create_regulator (&server_regulator_data, &server_pool,
				server_worker, server_worker_init);
  return true;
}

#ifdef RPC
/* Register and run the ZFS protocol server.  This function never returns
   (unless error occurs).  */

void
register_server ()
{
  SVCXPRT *udp;
  SVCXPRT *tcp;

  pmap_unset (ZFS_PROGRAM, ZFS_VERSION);

  udp = svcudp_create (RPC_ANYSOCK);
  if (udp == NULL)
    {
      message (-1, stderr, "cannot create udp service.\n");
      return;
    }
  if (!svc_register (udp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_UDP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, udp).\n",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

  tcp = svctcp_create (RPC_ANYSOCK, 0, 0);
  if (tcp == NULL)
    {
      message (-1, stderr, "cannot create tcp service.\n");
      return;
    }
  if (!svc_register (tcp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_TCP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, tcp).\n",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

#if 0
  svc_run ();
#endif
  message (-1, stderr, "svc_run returned.\n");

#if 0
  /* This does not help :-( */
  svc_destroy (udp);
  svc_destroy (tcp);
  svc_unregister (ZFS_PROGRAM, ZFS_VERSION);
#endif
}

#else

/* Main function of the main (i.e. listening) server thread.  */

static void *
server_main (void * ATTRIBUTE_UNUSED data)
{
  struct pollfd *pfd;
  int i, n;
  ssize_t r;
  int accept_connections;
  time_t now;
  static char dummy[ZFS_MAXDATA];

  pfd = (struct pollfd *) xmalloc (max_nfd * sizeof (struct pollfd));
  accept_connections = 1;

  while (running)
    {
      pthread_mutex_lock (&active_mutex);
      for (i = 0; i < nactive; i++)
	{
	  pfd[i].fd = active[i]->fd;
	  pfd[i].events = CAN_READ;
	}
      if (accept_connections)
	{
	  pfd[nactive].fd = main_socket;
	  pfd[nactive].events = CAN_READ;
	}
      n = nactive;
      pthread_mutex_unlock (&active_mutex);

      message (2, stderr, "Polling %d sockets\n", n + accept_connections);
      r = poll (pfd, n + accept_connections, -1);
      message (2, stderr, "Poll returned %d, errno=%d\n", r, errno);
      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, server_main exiting\n", strerror (errno));
	  free (pfd);
	  return NULL;
	}

      if (!running)
	{
	  message (2, stderr, "Terminating\n");
	  close (main_socket);
	  accept_connections = 0;

	  /* Close idle file descriptors and free their memory.  */
	  pthread_mutex_lock (&active_mutex);
	  for (i = nactive - 1; i >= 0; i--)
	    {
	      server_fd_data_t *fd_data = active[i];

	      pthread_mutex_lock (&fd_data->mutex);
	      if (fd_data->busy == 0)
		close_active_fd (i);
	      pthread_mutex_unlock (&fd_data->mutex);
	    }
	  pthread_mutex_unlock (&active_mutex);
	  free (pfd);
	  message (2, stderr, "Terminating...\n");
	  return NULL;
	}

      if (r <= 0)
	continue;

      now = time (NULL);

      /* Decrease the number of (unprocessed) sockets with events
	 if there were events on main socket.  */
      if (pfd[n].revents)
	r--;

      pthread_mutex_lock (&active_mutex);
      for (i = nactive - 1; i >= 0 && r > 0; i--)
	{
	  server_fd_data_t *fd_data = &server_fd_data[pfd[i].fd];

	  message (2, stderr, "FD %d revents %d\n", pfd[i].fd, pfd[i].revents);
	  if ((pfd[i].revents & CANNOT_RW)
	      || ((fd_data->flags & SERVER_FD_CLOSE)
		  && fd_data->busy == 0
		  && fd_data->read == 0))
	    {
	      pthread_mutex_lock (&fd_data->mutex);
	      close_active_fd (i);
	      pthread_mutex_unlock (&fd_data->mutex);
	    }
	  else if (pfd[i].revents & CAN_READ)
	    {
	      fd_data->last_use = now;
	      if (fd_data->read < 4)
		{
		  ssize_t r;

		  pthread_mutex_lock (&fd_data->mutex);
		  if (fd_data->ndc == 0)
		    {
		      dc_create (&fd_data->dc[0], ZFS_MAX_REQUEST_LEN);
		      fd_data->ndc++;
		    }
		  pthread_mutex_unlock (&fd_data->mutex);

		  r = read (fd_data->fd, fd_data->dc[0].buffer + fd_data->read,
			    4 - fd_data->read);
		  if (r <= 0)
		    {
		      pthread_mutex_lock (&fd_data->mutex);
		      close_active_fd (i);
		      pthread_mutex_unlock (&fd_data->mutex);
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
		      pthread_mutex_lock (&fd_data->mutex);
		      close_active_fd (i);
		      pthread_mutex_unlock (&fd_data->mutex);
		    }
		  else
		    {
		      fd_data->read += r;

		      if (fd_data->dc[0].max_length == fd_data->read)
			{
			  unsigned int generation;
			  DC *dc;

			  pthread_mutex_lock (&fd_data->mutex);
			  generation = fd_data->generation;
			  dc = &fd_data->dc[0];
			  fd_data->read = 0;
			  fd_data->busy++;
			  fd_data->ndc--;
			  if (fd_data->ndc > 0)
			    fd_data->dc[0] = fd_data->dc[fd_data->ndc];
			  pthread_mutex_unlock (&fd_data->mutex);

			  /* We have read complete request, dispatch it.  */
			  server_dispatch (fd_data, dc, generation);
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
	      socklen_t ca_len;

retry_accept:
	      s = accept (main_socket, (struct sockaddr *) &ca, &ca_len);

	      if ((s < 0 && errno == EMFILE)
		  || (s >= 0 && nactive == max_server_sockets))
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
		      if (s > 0)
			close (s);
		      pthread_mutex_unlock (&active_mutex);
		      continue;
		    }
		  else
		    {
		      server_fd_data_t *fd_data = active[index];

		      /* Close file descriptor unused for the longest time.  */
		      pthread_mutex_lock (&fd_data->mutex);
		      close_active_fd (index);
		      pthread_mutex_unlock (&fd_data->mutex);
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
		  pthread_mutex_lock (&server_fd_data[s].mutex);
		  init_fd_data (s);
		  pthread_mutex_unlock (&server_fd_data[s].mutex);
		}
	    }
	}
      pthread_mutex_unlock (&active_mutex);
    }

  free (pfd);
  return NULL;
}

/* Create a listening socket and start the main server thread.  */

bool
server_start ()
{
  socklen_t socket_options;
  struct sockaddr_in sa;

  /* Create a server socket.  */
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

  /* Bind the server socket to ZFS_PORT.  */
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

  /* Create the main server thread.  */
  if (pthread_create (&main_server_thread, NULL, server_main, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
      free (server_fd_data);
      close (main_socket);
      return false;
    }

  return true;
}

/* Initialize information about network file descriptors.  */

bool
server_init_fd_data ()
{
  int i;

  if (pthread_mutex_init (&active_mutex, NULL))
    {
      message (-1, stderr, "pthread_mutex_init() failed\n");
      return false;
    }

  server_fd_data = (server_fd_data_t *) xcalloc (max_nfd,
						 sizeof (server_fd_data_t));
  for (i = 0; i < max_nfd; i++)
    {
      if (pthread_mutex_init (&server_fd_data[i].mutex, NULL))
	{
	  message (-1, stderr, "pthread_mutex_init() failed\n");
	  free (server_fd_data);
	  return false;
	}
      server_fd_data[i].fd = -1;
    }

  nactive = 0;
  active = (server_fd_data_t **) xmalloc (max_nfd * sizeof (server_fd_data_t));

  return true;
}

/* Destroy information about network file descriptors.  */

void
server_destroy_fd_data ()
{
  int i;

  /* Close connected sockets.  */
  pthread_mutex_lock (&active_mutex);
  for (i = nactive - 1; i >= 0; i--)
    {
      server_fd_data_t *fd_data = active[i];
      pthread_mutex_lock (&fd_data->mutex);
      close_active_fd (i);
      pthread_mutex_unlock (&fd_data->mutex);
    }
  pthread_mutex_unlock (&active_mutex);
  pthread_mutex_destroy (&active_mutex);

  for (i = 0; i < max_nfd; i++)
    pthread_mutex_destroy (&server_fd_data[i].mutex);

  free (active);
  free (server_fd_data);
}

#endif

/* Terminate server threads and destroy data structures.  */

void
server_cleanup ()
{
  int i;

  /* Tell each thread waiting for reply that we are exiting.  */
  for (i = nactive - 1; i >= 0; i--)
    {
      server_fd_data_t *fd_data = active[i];
      void **slot;

      pthread_mutex_lock (&fd_data->mutex);
      HTAB_FOR_EACH_SLOT (fd_data->waiting4reply, slot,
	{
	  waiting4reply_data *data = *(waiting4reply_data **) slot;

	  data->t->u.server.retval = ZFS_EXITING;
	  semaphore_up (&data->t->sem, 1);
	});
      pthread_mutex_unlock (&fd_data->mutex);
    }

  pthread_kill (server_regulator_data.thread_id, SIGUSR1);
  thread_pool_destroy (&server_pool);
  server_destroy_fd_data ();
}
