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

/* Thread ID of the main server thread (thread receiving data from sockets).  */
pthread_t main_server_thread;

/* Key for server thread specific data.  */
pthread_key_t server_thread_data_key;

/* File descriptor of the main (i.e. listening) socket.  */
static int main_socket;

/* The array of data for each file descriptor.  */
static server_fd_data_t *server_fd_data;

/* Array of pointers to data of active file descriptors.  */
static server_fd_data_t **active;

/* Number of active file descriptors.  */
static int nactive;

/* Close an active file descriptor on index I in ACTIVE.  */

static void
close_active_fd (int i)
{
  int fd = active[i]->fd;
  int j;

#ifdef ENABLE_CHECKING
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
}

/* Initialize server thread T.  */

static void
server_worker_init (thread *t)
{
  t->u.server.reply = (char *) xmalloc (ZFS_MAX_REQUEST_LEN);
}

/* Cleanup server thread DATA.  */

static void
server_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  free (t->u.server.reply);
}

/* The main function of the server thread.  */

static void *
server_worker (void *data)
{
  thread *t = (thread *) data;
  server_thread_data *td = &t->u.server;
  server_fd_data_t *d;

  pthread_cleanup_push (server_worker_cleanup, data);
  pthread_setspecific (server_thread_data_key, data);

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

      d = td->fd_data;
      /* FIXME: process the request */
      /* 1. decode request */
      /* 2. call appropriate routine */
      /* 3. encode reply */
      pthread_mutex_lock (&d->mutex);
      if (d->fd >= 0 && d->generation == td->generation)
	{
	  /* 4. send a reply */
	}
      if (running)
	{
	  if (d->ndc < MAX_FREE_BUFFERS_PER_SERVER_FD)
	    {
	      /* Add the buffer to the queue.  */
	      d->dc[d->ndc] = td->dc;
	      d->ndc++;
	    }
	  else
	    {
	      /* Free the buffer.  */
	      dc_destroy (&td->dc);
	    }
	}
      else
	{
	  d->busy--;
	  if (d->busy == 0 && d->fd >= 0)
	    close_active_fd (d->fd);
	}
      pthread_mutex_unlock (&d->mutex);

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

static int
server_dispatch (server_fd_data_t *fd_data, DC *dc, unsigned int generation)
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
  server_pool.threads[index].t.u.server.fd_data = fd_data;
  server_pool.threads[index].t.u.server.dc = *dc;
  server_pool.threads[index].t.u.server.generation = generation;
  pthread_mutex_unlock (&server_pool.threads[index].t.mutex);

  pthread_mutex_unlock (&server_pool.idle.mutex);

  return 0;
}

#endif

/* Create server threads and related threads.  */

int
create_server_threads ()
{
  int i;

  if (pthread_key_create (&server_thread_data_key, NULL))
    return 0;
  
  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&server_pool, 256, 4, 16);

  pthread_mutex_lock (&server_pool.idle.mutex);
  pthread_mutex_lock (&server_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      create_idle_thread (&server_pool, server_worker, NULL);
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
  int i, n, r;
  int accept_connections;
  time_t now;

  nactive = 0;
  active = (server_fd_data_t **) xmalloc (getdtablesize ()
				       * sizeof (server_fd_data_t));
  pfd = (struct pollfd *) xmalloc (getdtablesize () * sizeof (struct pollfd));
  accept_connections = 1;

  while (running)
    {
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

      r = poll (pfd, nactive + 1, -1);
      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, server_main exiting\n", strerror (errno));
	  return NULL;
	}

      if (!running)
	{
	  close (main_socket);
	  accept_connections = 0;

	  /* Close idle file descriptors and free their memory.  */
	  for (i = 0; i < nactive; i++)
	    {
	      /* FIXME */
	    }
	  return NULL;
	}

      if (r == 0)
	continue;

      now = time (NULL);

      /* Remember the number of active file descriptors because nactive may
	 change until the end of the body of "while (running)" loop.  */
      n = nactive;

      /* Decrease the number of (unprocessed) sockets with events
	 if there were events on main socket.  */
      if (pfd[nactive].revents)
	r--;

      for (i = n - 1; i >= 0 && r > 0; i--)
	{
	  server_fd_data_t *d = &server_fd_data[pfd[i].fd];

	  if (pfd[i].revents & CANNOT_RW)
	    {
	      pthread_mutex_lock (&active[i]->mutex);
	      close_active_fd (i);
	      pthread_mutex_unlock (&active[i]->mutex);
	    }
	  else if (pfd[i].revents & CAN_READ)
	    {
	      d->last_use = now;
	      if (d->read < 4)
		{
		  ssize_t r;

		  pthread_mutex_lock (&d->mutex);
		  if (d->ndc == 0)
		    {
		      dc_create (&d->dc[0], ZFS_MAX_REQUEST_LEN);
		      d->ndc++;
		    }
		  pthread_mutex_unlock (&d->mutex);
		  r = read (d->fd, d->dc[0].start + d->read, 4 - d->read);
		  if (r < 0)
		    {
		      pthread_mutex_lock (&active[i]->mutex);
		      close_active_fd (i);
		      pthread_mutex_unlock (&active[i]->mutex);
		    }
		  else
		    d->read += r;

		  if (d->read == 4)
		    d->length = GET_UINT (d->dc[0].start);
		}
	      else
		{
		  r = read (d->fd, d->dc[0].start + d->read,
			    d->length - 4);
		  if (r < 0)
		    {
		      pthread_mutex_lock (&active[i]->mutex);
		      close_active_fd (i);
		      pthread_mutex_unlock (&active[i]->mutex);
		    }
		  else
		    {
		      d->read += r;

		      if (d->read == d->length)
			{
			  unsigned int generation;
			  DC *dc;

			  pthread_mutex_lock (&d->mutex);
			  generation = d->generation;
			  dc = &d->dc[0];
			  d->busy++;
			  d->ndc--;
			  if (d->ndc > 0)
			    d->dc[0] = d->dc[d->ndc];
			  pthread_mutex_unlock (&d->mutex);

			  /* We have read complete request so dispatch it.  */
			  server_dispatch (d, dc, generation);
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
		      if (s > 0)
			close (s);
		    }
		  else
		    {
		      /* Close file descriptor unused for the longest time.  */
		      pthread_mutex_lock (&active[index]->mutex);
		      close_active_fd (index);
		      pthread_mutex_unlock (&active[index]->mutex);
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
		  /* Set the server's data.  */
		  active[nactive]->fd = s;
		  active[nactive]->read = 0;
		  if (active[nactive]->ndc == 0)
		    {
		      dc_create (&active[nactive]->dc[0], ZFS_MAX_REQUEST_LEN);
		      active[nactive]->ndc++;
		    }
		  active[nactive]->last_use = now;
		  active[nactive]->generation++;
		  active[nactive]->busy = 0;
		  nactive++;
		}
	    }
	}
    }

  return NULL;
}

/* Create a listening socket and start the main server thread.  */

int
server_start ()
{
  socklen_t socket_options;
  struct sockaddr_in sa;
  int i, n;

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

  n = getdtablesize ();
  server_fd_data = (server_fd_data_t *) xcalloc (n, sizeof (server_fd_data_t));
  for (i = 0; i < n; i++)
    if (pthread_mutex_init (&server_fd_data[i].mutex, NULL))
      {
	message (-1, stderr, "pthread_mutex_init() failed\n");
	free (server_fd_data);
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

#endif

/* Terminate server threads.  */

int
server_cleanup ()
{
  return 0;
}
