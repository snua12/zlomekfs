/* Client thread functions.
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "pthread.h"
#include "constant.h"
#include "semaphore.h"
#include "data-coding.h"
#include "client.h"
#include "log.h"
#include "util.h"
#include "memory.h"
#include "thread.h"
#include "zfs_prot.h"
#include "config.h"

/* Pool of client threads.  */
static thread_pool client_pool;

/* Data for client pool regulator.  */
thread_pool_regulator_data client_regulator_data;

#include <time.h>
#include "memory.h"
#include "node.h"
#include "hashtab.h"
#include "alloc-pool.h"

/* Data for a client socket.  */
typedef struct client_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor of the socket */
  unsigned int read;		/* number of bytes already read */
  unsigned int busy;		/* number of threads using file descriptor */

  /* Unused data coding buffers for the file descriptor.  */
  DC dc[MAX_FREE_BUFFERS_PER_ACTIVE_FD];
  int ndc;
} client_fd_data_t;

/* Thread ID of the main client thread (thread receiving data from sockets).  */
pthread_t main_client_thread;

/* This mutex is locked when main client thread is in poll.  */
pthread_mutex_t main_client_thread_in_syscall;

/* File descriptor of file communicating with kernel.  */
static int kernel_file;

client_fd_data_t client_data;

/* Send a reply.  */

static void
send_reply (thread *t)
{
  message (2, stderr, "sending reply\n");
  zfsd_mutex_lock (&client_data.mutex);
  if (!full_write (kernel_file, t->dc.buffer, t->dc.cur_length))
    {
    }
  zfsd_mutex_unlock (&client_data.mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (thread *t, uint32_t request_id, int status)
{
  start_encoding (&t->dc);
  encode_direction (&t->dc, DIR_REPLY);
  encode_request_id (&t->dc, request_id);
  encode_status (&t->dc, status);
  finish_encoding (&t->dc);
  send_reply (t);
}

/* Initialize client thread T.  */

void
client_worker_init (thread *t)
{
  dc_create (&t->dc_call, ZFS_MAX_REQUEST_LEN);
}

/* Cleanup client thread DATA.  */

void
client_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (&t->dc_call);
}

/* The main function of the client thread.  */

static void *
client_worker (void *data)
{
  thread *t = (thread *) data;
  uint32_t request_id;
  uint32_t fn;

  thread_disable_signals ();

  pthread_cleanup_push (client_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);

  while (1)
    {
      /* Wait until client_dispatch wakes us up.  */
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
	    if (!decode_##ARGS (&t->dc, &t->args.FUNCTION)		      \
		|| !finish_decoding (&t->dc))				      \
	      {								      \
		send_error_reply (t, request_id, ZFS_INVALID_REQUEST);	      \
		goto out;						      \
	      }								      \
	    start_encoding (&t->dc);					      \
	    encode_direction (&t->dc, DIR_REPLY);			      \
	    encode_request_id (&t->dc, request_id);			      \
	    zfs_proc_##FUNCTION##_server (&t->args.FUNCTION, t);	      \
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
      zfsd_mutex_lock (&client_data.mutex);
      if (get_running ())
	{
	  if (client_data.ndc < MAX_FREE_BUFFERS_PER_ACTIVE_FD)
	    {
	      /* Add the buffer to the queue.  */
	      client_data.dc[client_data.ndc] = t->dc;
	      client_data.ndc++;
	    }
	  else
	    {
	      /* Free the buffer.  */
	      dc_destroy (&t->dc);
	    }
	}
      zfsd_mutex_unlock (&client_data.mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&client_pool.idle.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
	{
	  queue_put (&client_pool.idle, t->index);
	  set_thread_state (t, THREAD_IDLE);
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (get_thread_state (t) != THREAD_DYING)
	    abort ();
#endif
	  zfsd_mutex_unlock (&client_pool.idle.mutex);
	  break;
	}
      zfsd_mutex_unlock (&client_pool.idle.mutex);
    }

  pthread_cleanup_pop (1);

  return data;
}

/* Function which gets a request and passes it to some client thread.
   It also regulates the number of client threads.  */

static void
client_dispatch (DC *dc)
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
      case DIR_REQUEST:
	/* Dispatch request.  */

	zfsd_mutex_lock (&client_pool.idle.mutex);

	/* Regulate the number of threads.  */
	thread_pool_regulate (&client_pool, client_worker, NULL);

	/* Select an idle thread and forward the request to it.  */
	index = queue_get (&client_pool.idle);
#ifdef ENABLE_CHECKING
	if (get_thread_state (&client_pool.threads[index].t) == THREAD_BUSY)
	  abort ();
#endif
	set_thread_state (&client_pool.threads[index].t, THREAD_BUSY);
	client_pool.threads[index].t.dc = *dc;

	/* Let the thread run.  */
	semaphore_up (&client_pool.threads[index].t.sem, 1);

	zfsd_mutex_unlock (&client_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */
	abort ();
    }
}

/* Create client threads and related threads.  */

bool
create_client_threads ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&client_pool, 256, 4, 16);

  zfsd_mutex_lock (&client_pool.idle.mutex);
  zfsd_mutex_lock (&client_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      create_idle_thread (&client_pool, client_worker, client_worker_init);
    }
  zfsd_mutex_unlock (&client_pool.empty.mutex);
  zfsd_mutex_unlock (&client_pool.idle.mutex);

  thread_pool_create_regulator (&client_regulator_data, &client_pool,
				client_worker, client_worker_init);
  return true;
}

/* Main function of the main (i.e. listening) client thread.  */

static void *
client_main (void * ATTRIBUTE_UNUSED data)
{
  struct pollfd pfd;
  ssize_t r;
  static char dummy[ZFS_MAXDATA];

  thread_disable_signals ();

  while (get_running ())
    {
      pfd.fd = kernel_file;
      pfd.events = CAN_READ;

      message (2, stderr, "Polling\n");
      zfsd_mutex_lock (&main_client_thread_in_syscall);
      r = poll (&pfd, 1, -1);
      zfsd_mutex_unlock (&main_client_thread_in_syscall);
      message (2, stderr, "Poll returned %d, errno=%d\n", r, errno);

      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, client_main exiting\n", strerror (errno));
	  break;
	}

      if (!get_running ())
	{
	  message (2, stderr, "Terminating\n");
	  break;
	}

      if (r <= 0)
	continue;

      message (2, stderr, "FD %d revents %d\n", pfd.fd, pfd.revents);
      if (pfd.revents & CANNOT_RW)
	break;

      if (pfd.revents & CAN_READ)
	{
	  if (client_data.read < 4)
	    {
	      ssize_t r;

	      zfsd_mutex_lock (&client_data.mutex);
	      if (client_data.ndc == 0)
		{
		  dc_create (&client_data.dc[0], ZFS_MAX_REQUEST_LEN);
		  client_data.ndc++;
		}
	      zfsd_mutex_unlock (&client_data.mutex);

	      r = read (client_data.fd, client_data.dc[0].buffer + client_data.read,
			4 - client_data.read);
	      if (r <= 0)
		break;

	      client_data.read += r;
	      if (client_data.read == 4)
		{
		  start_decoding (&client_data.dc[0]);
		}
	    }
	  else
	    {
	      if (client_data.dc[0].max_length <= client_data.dc[0].size)
		{
		  r = read (client_data.fd,
			    client_data.dc[0].buffer + client_data.read,
			    client_data.dc[0].max_length - client_data.read);
		}
	      else
		{
		  int l;

		  l = client_data.dc[0].max_length - client_data.read;
		  if (l > ZFS_MAXDATA)
		    l = ZFS_MAXDATA;
		  r = read (client_data.fd, dummy, l);
		}

	      if (r <= 0)
		break;

	      client_data.read += r;
	      if (client_data.dc[0].max_length == client_data.read)
		{
		  DC *dc;

		  zfsd_mutex_lock (&client_data.mutex);
		  dc = &client_data.dc[0];
		  client_data.read = 0;
		  client_data.busy++;
		  client_data.ndc--;
		  if (client_data.ndc > 0)
		    client_data.dc[0] = client_data.dc[client_data.ndc];
		  zfsd_mutex_unlock (&client_data.mutex);

		  /* We have read complete request, dispatch it.  */
		  client_dispatch (dc);
		}
	    }
	}
    }

  if (client_data.busy == 0)
    close (kernel_file);
  message (2, stderr, "Terminating...\n");
  return NULL;
}

/* Create a listening socket and start the main client thread.  */

bool
client_start ()
{
#if 0
  socklen_t socket_options;
  struct sockaddr_in sa;
#endif

  zfsd_mutex_init (&client_data.mutex);

  /* Open connection with kernel.  */
  kernel_file = open (kernel_file_name, O_RDWR);
  if (kernel_file < 0)
    {
      message (-1, stderr, "open(): %s\n", strerror (errno));
      return false;
    }

  /* Create the main client thread.  */
  if (pthread_create (&main_client_thread, NULL, client_main, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
      close (kernel_file);
      return false;
    }

  return true;
}

/* Terminate client threads and destroy data structures.  */

void
client_cleanup ()
{
  thread_pool_destroy (&client_pool);
  zfsd_mutex_destroy (&client_data.mutex);
}
