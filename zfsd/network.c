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
static server_fd_data_t *server_fd_data;

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

  server_fd_data[fd].waiting4reply_pool
    = create_alloc_pool ("waiting4reply_data",
			 sizeof (waiting4reply_data), 30);
  server_fd_data[fd].waiting4reply
    = htab_create (30, waiting4reply_hash, waiting4reply_eq,
		   NULL);
}

/* Close an active file descriptor on index I in ACTIVE.  */

static void
close_active_fd (int i)
{
  int fd = active[i]->fd;
  int j;
  void **slot;

  message (2, stderr, "Closing FD %d\n", fd);
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&active_mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
#endif
  close (active[i]->fd);
  active[i]->generation++;
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
      pthread_mutex_unlock (&data->t->mutex);
    });
  htab_destroy (server_fd_data[fd].waiting4reply);
  free_alloc_pool (server_fd_data[fd].waiting4reply_pool);
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

/* Authenticate connection with node NOD on socket FD.  */

static bool
node_authenticate (node nod, int fd)
{
  auth_stage1_args args1;
  auth_stage2_args args2;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&nod->mutex) == 0)
    abort ();
#endif

#if 0
  /* FIXME: really do authentication; currently the functions are empty.  */
  if (zfs_proc_auth_stage1_client (t, &args1, nod) != ZFS_OK)
    goto node_authenticate_error;

  if (zfs_proc_auth_stage2_client (t, &args2, nod) != ZFS_OK)
    goto node_authenticate_error;
#endif
  
  nod->auth = AUTHENTICATION_DONE;
  return true;

node_authenticate_error:
  nod->fd = -1;
  nod->auth = AUTHENTICATION_NONE;
  nod->conn = CONNECTION_NONE;
  close (fd);
  return false;
}

/* Connect to node NOD, return open file descriptor.  */

static int
node_connect (node nod)
{
  struct addrinfo *addr, *a;
  int s;
  int err;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&nod->mutex) == 0)
    abort ();
#endif

  /* Lookup the IP address.  */
  if ((err = getaddrinfo (nod->name, NULL, NULL, &addr)) != 0)
    {
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

		/* Connect the server socket to ZFS_PORT.  */
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

		/* Connect the server socket to ZFS_PORT.  */
		((struct sockaddr_in6 *)a->ai_addr)->sin6_port
		  = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) >= 0)
		  goto node_connected;
	      }
	    break;
	}
    }

  message (-1, stderr, "Could not connect to %s\n", nod->name);
  close (s);
  freeaddrinfo (addr);
  return -1;

node_connected: 
  freeaddrinfo (addr);
  nod->fd = s;
  nod->auth = AUTHENTICATION_NONE;
  nod->conn = CONNECTION_FAST; /* FIXME */
  message (2, stderr, "FD %d connected to %s\n", s, nod->name);
  return s;
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

      message (2, stderr, "Sending request:\n");
      for (i = 0; i < len; i++)
	fprintf (stderr, "%02x ", buf[i]);
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

static void
really_send_request (thread *t, int fd, uint32_t request_id)
{
  server_thread_data *td = &t->u.server;
  void **slot;
  waiting4reply_data *wd;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[fd].mutex) == 0)
    abort ();
#endif

  /* Here, server_fd_data[fd].mutex is locked.  */

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
  if (!safe_write (fd, td->dc_call.buffer, td->dc_call.cur_length))
    {
      pthread_mutex_unlock (&server_fd_data[fd].mutex);
      td->retval = ZFS_CONNECTION_CLOSED;
      htab_clear_slot (server_fd_data[fd].waiting4reply, slot);
      return;
    }
  pthread_mutex_unlock (&server_fd_data[fd].mutex);

  /* Wait for reply.  */
  pthread_mutex_lock (&t->mutex);
  
  /* Decode return value.  */
  if (!decode_status (&td->dc, &td->retval))
    td->retval = ZFS_INVALID_REPLY;
}

/* Send request with request id REQUEST_ID using data in thread T to node NOD
   and wait for reply.  Try to connect to node NOD if not connected.  */

void
send_request (thread *t, uint32_t request_id, node nod)
{
  server_thread_data *td = &t->u.server;
  int fd;

  pthread_mutex_lock (&nod->mutex);
  fd = nod->fd;
  if (fd >= 0)
    {
      pthread_mutex_lock (&server_fd_data[fd].mutex);
      if (nod->generation == server_fd_data[fd].generation)
	{
	  pthread_mutex_unlock (&nod->mutex);
	  really_send_request (t, fd, request_id);
	  return;
	}
      pthread_mutex_unlock (&server_fd_data[fd].mutex);
    }

  fd = node_connect (nod);
  if (fd < 0)
    {
      td->retval = ZFS_COULD_NOT_CONNECT;
      pthread_mutex_unlock (&nod->mutex);
      return;
    }

  pthread_mutex_lock (&active_mutex);
  pthread_mutex_lock (&server_fd_data[fd].mutex);
  init_fd_data (fd);
  pthread_kill (main_server_thread, SIGUSR1);
  nod->generation = server_fd_data[fd].generation;
  pthread_mutex_unlock (&active_mutex);

  if (!node_authenticate (nod, fd))
    {
      td->retval = ZFS_COULD_NOT_AUTH;
      pthread_mutex_unlock (&nod->mutex);
      return;
    }
  
  pthread_mutex_unlock (&nod->mutex);
  really_send_request (t, fd, request_id);
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
      pthread_mutex_lock (&t->mutex);

#ifdef ENABLE_CHECKING
      if (t->state == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (t->state == THREAD_DYING)
	return data;

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
      switch (fn)
	{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
	  case ZFS_PROC_##NAME:						\
	    if (!decode_##ARGS_TYPE (&td->dc, &td->args.FUNCTION)	\
		|| !finish_decoding (&td->dc))				\
	      {								\
		send_error_reply (td, request_id, ZFS_INVALID_REQUEST);	\
		goto out;						\
	      }								\
	    start_encoding (&td->dc);					\
	    encode_direction (&td->dc, DIR_REPLY);			\
	    encode_request_id (&td->dc, request_id);			\
	    zfs_proc_##FUNCTION##_server (&td->args.FUNCTION, &td->dc);	\
	    finish_encoding (&td->dc);					\
	    send_reply (td);						\
	    break;
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
		
	  default:
	    send_error_reply (td, request_id, ZFS_UNKNOWN_FUNCTION);
	    goto out;
	}

out:
      fd_data = td->fd_data;
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

	    if (!decode_request_id (dc, &request_id))
	      {
		/* TODO: log too short packet.  */
		break;
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
		break;
	      }

	    data = *(waiting4reply_data **) slot;
	    data->t->u.server.dc = *dc;
	    htab_clear_slot (fd_data->waiting4reply, slot);

	    /* Let the thread run again.  */
	    pthread_mutex_unlock (&data->t->mutex);
	    pool_free (fd_data->waiting4reply_pool, data);
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
	pthread_mutex_unlock (&server_pool.threads[index].t.mutex);

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

int
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
  return 1;
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

  pfd = (struct pollfd *) xmalloc (getdtablesize () * sizeof (struct pollfd));
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
      message (2, stderr, "Poll returned %d\n", r);
      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, server_main exiting\n", strerror (errno));
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
	  if (pfd[i].revents & CANNOT_RW)
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

  return NULL;
}

/* Create a listening socket and start the main server thread.  */

int
server_start ()
{
  socklen_t socket_options;
  struct sockaddr_in sa;

  /* Create a server socket.  */
  main_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (main_socket < 0)
    {
      message (-1, stderr, "socket(): %s\n", strerror (errno));
      return 0;
    }

  /* Reuse the port.  */
  socket_options = 1;
  if (setsockopt (main_socket, SOL_SOCKET, SO_REUSEADDR, &socket_options,
	      sizeof (socket_options)) != 0)
    {
      message (-1, stderr, "setsockopt(): %s\n", strerror (errno));
      close (main_socket);
      return 0;
    }

  /* Bind the server socket to ZFS_PORT.  */
  sa.sin_family = AF_INET;
  sa.sin_port = htons (ZFS_PORT);
  sa.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (main_socket, (struct sockaddr *) &sa, sizeof (sa)))
    {
      message (-1, stderr, "bind(): %s\n", strerror (errno));
      close (main_socket);
      return 0;
    }

  /* Set the queue for incoming connections.  */
  if (listen (main_socket, SOMAXCONN) != 0)
    {
      message (-1, stderr, "listen(): %s\n", strerror (errno));
      close (main_socket);
      return 0;
    }

  /* Create the main server thread.  */
  if (pthread_create (&main_server_thread, NULL, server_main, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
      free (server_fd_data);
      close (main_socket);
      return 0;
    }

  return 1;
}

/* Initialize information about network file descriptors.  */

int
server_init_fd_data ()
{
  int i, n;

  if (pthread_mutex_init (&active_mutex, NULL))
    {
      message (-1, stderr, "pthread_mutex_init() failed\n");
      return 0;
    }

  n = getdtablesize ();
  server_fd_data = (server_fd_data_t *) xcalloc (n, sizeof (server_fd_data_t));
  for (i = 0; i < n; i++)
    if (pthread_mutex_init (&server_fd_data[i].mutex, NULL))
      {
	message (-1, stderr, "pthread_mutex_init() failed\n");
	free (server_fd_data);
	return 0;
      }

  nactive = 0;
  active = (server_fd_data_t **) xmalloc (getdtablesize ()
					  * sizeof (server_fd_data_t));

  return 1;
}

#endif

/* Terminate server threads.  */

int
server_cleanup ()
{
  /* TODO: kill threads and wait 4 threads to die.  */
  return 0;
}
