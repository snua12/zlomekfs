/* Functions for threads communicating with kernel.
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
#include "hashtab.h"
#include "alloc-pool.h"
#include "data-coding.h"
#include "kernel.h"
#include "log.h"
#include "node.h"
#include "util.h"
#include "memory.h"
#include "thread.h"
#include "zfs_prot.h"
#include "config.h"

/* Data for a kernel socket.  */
typedef struct kernel_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor of the socket */
  unsigned int read;		/* number of bytes already read */
  unsigned int busy;		/* number of threads using file descriptor */

  /* Unused data coding buffers for the file descriptor.  */
  DC dc[MAX_FREE_BUFFERS_PER_ACTIVE_FD];
  int ndc;
} kernel_fd_data_t;

/* Pool of kernel threads (threads communicating with kernel).  */
thread_pool kernel_pool;

/* File descriptor of file communicating with kernel.  */
static int kernel_file;

kernel_fd_data_t kernel_data;

/* Send a reply.  */

static void
send_reply (thread *t)
{
  message (2, stderr, "sending reply\n");
  zfsd_mutex_lock (&kernel_data.mutex);
  if (!full_write (kernel_file, t->u.kernel.dc.buffer,
		   t->u.kernel.dc.cur_length))
    {
    }
  zfsd_mutex_unlock (&kernel_data.mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (thread *t, uint32_t request_id, int32_t status)
{
  start_encoding (&t->u.kernel.dc);
  encode_direction (&t->u.kernel.dc, DIR_REPLY);
  encode_request_id (&t->u.kernel.dc, request_id);
  encode_status (&t->u.kernel.dc, status);
  finish_encoding (&t->u.kernel.dc);
  send_reply (t);
}

/* Initialize kernel thread T.  */

static void
kernel_worker_init (thread *t)
{
  dc_create (&t->dc_call, ZFS_MAX_REQUEST_LEN);
}

/* Cleanup kernel thread DATA.  */

static void
kernel_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (&t->dc_call);
}

/* The main function of the kernel thread.  */

static void *
kernel_worker (void *data)
{
  thread *t = (thread *) data;
  uint32_t request_id;
  uint32_t fn;

  thread_disable_signals ();

  pthread_cleanup_push (kernel_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);

  while (1)
    {
      /* Wait until kernel_dispatch wakes us up.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (get_thread_state (t) == THREAD_DYING)
	break;

      if (!decode_request_id (&t->u.kernel.dc, &request_id))
	{
	  /* TODO: log too short packet.  */
	  goto out;
	}

      if (t->u.kernel.dc.max_length > t->u.kernel.dc.size)
	{
	  send_error_reply (t, request_id, ZFS_REQUEST_TOO_LONG);
	  goto out;
	}

      if (!decode_function (&t->u.kernel.dc, &fn))
	{
	  send_error_reply (t, request_id, ZFS_INVALID_REQUEST);
	  goto out;
	}

      message (2, stderr, "REQUEST: ID=%u function=%u\n", request_id, fn);
      switch (fn)
	{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
	  case ZFS_PROC_##NAME:						\
	    if (!decode_##ARGS (&t->u.kernel.dc,			\
				&t->u.kernel.args.FUNCTION)		\
		|| !finish_decoding (&t->u.kernel.dc))			\
	      {								\
		send_error_reply (t, request_id, ZFS_INVALID_REQUEST);	\
		goto out;						\
	      }								\
	    start_encoding (&t->u.kernel.dc);				\
	    encode_direction (&t->u.kernel.dc, DIR_REPLY);		\
	    encode_request_id (&t->u.kernel.dc, request_id);		\
	    zfs_proc_##FUNCTION##_server (&t->u.kernel.args.FUNCTION,	\
					  &t->u.kernel.dc,		\
					  &t->u.kernel, true);		\
	    finish_encoding (&t->u.kernel.dc);				\
	    send_reply (t);						\
	    break;
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

	  default:
	    send_error_reply (t, request_id, ZFS_UNKNOWN_FUNCTION);
	    goto out;
	}

out:
      zfsd_mutex_lock (&kernel_data.mutex);
      if (kernel_data.ndc < MAX_FREE_BUFFERS_PER_ACTIVE_FD)
	{
	  /* Add the buffer to the queue.  */
	  kernel_data.dc[kernel_data.ndc] = t->u.kernel.dc;
	  kernel_data.ndc++;
	}
      else
	{
	  /* Free the buffer.  */
	  dc_destroy (&t->u.kernel.dc);
	}
      zfsd_mutex_unlock (&kernel_data.mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&kernel_pool.idle.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
	{
	  queue_put (&kernel_pool.idle, &t->index);
	  set_thread_state (t, THREAD_IDLE);
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (get_thread_state (t) != THREAD_DYING)
	    abort ();
#endif
	  zfsd_mutex_unlock (&kernel_pool.idle.mutex);
	  break;
	}
      zfsd_mutex_unlock (&kernel_pool.idle.mutex);
    }

  pthread_cleanup_pop (1);

  return NULL;
}

/* Function which gets a request and passes it to some kernel thread.
   It also regulates the number of kernel threads.  */

static void
kernel_dispatch (DC *dc)
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

	zfsd_mutex_lock (&kernel_pool.idle.mutex);

	/* Regulate the number of threads.  */
	if (kernel_pool.idle.nelem == 0)
	  thread_pool_regulate (&kernel_pool);

	/* Select an idle thread and forward the request to it.  */
	queue_get (&kernel_pool.idle, &index);
#ifdef ENABLE_CHECKING
	if (get_thread_state (&kernel_pool.threads[index].t) == THREAD_BUSY)
	  abort ();
#endif
	set_thread_state (&kernel_pool.threads[index].t, THREAD_BUSY);
	kernel_pool.threads[index].t.from_sid = this_node->id;
	  /* FIXME: race condition? */
	kernel_pool.threads[index].t.u.kernel.dc = *dc;

	/* Let the thread run.  */
	semaphore_up (&kernel_pool.threads[index].t.sem, 1);

	zfsd_mutex_unlock (&kernel_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */
	abort ();
    }
}

/* Main function of the main (i.e. listening) kernel thread.  */

static void *
kernel_main (ATTRIBUTE_UNUSED void *data)
{
  struct pollfd pfd;
  ssize_t r;
  static char dummy[ZFS_MAXDATA];

  thread_disable_signals ();

  while (!thread_pool_terminate_p (&kernel_pool))
    {
      pfd.fd = kernel_file;
      pfd.events = CAN_READ;

      message (2, stderr, "Polling\n");
      zfsd_mutex_lock (&kernel_pool.main_in_syscall);
      r = poll (&pfd, 1, -1);
      zfsd_mutex_unlock (&kernel_pool.main_in_syscall);
      message (2, stderr, "Poll returned %d, errno=%d\n", r, errno);

      if (r < 0 && errno != EINTR)
	{
	  message (-1, stderr, "%s, kernel_main exiting\n", strerror (errno));
	  break;
	}

      if (thread_pool_terminate_p (&kernel_pool))
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
	  if (kernel_data.read < 4)
	    {
	      zfsd_mutex_lock (&kernel_data.mutex);
	      if (kernel_data.ndc == 0)
		{
		  dc_create (&kernel_data.dc[0], ZFS_MAX_REQUEST_LEN);
		  kernel_data.ndc++;
		}
	      zfsd_mutex_unlock (&kernel_data.mutex);

	      r = read (kernel_data.fd, kernel_data.dc[0].buffer + kernel_data.read,
			4 - kernel_data.read);
	      if (r <= 0)
		break;

	      kernel_data.read += r;
	      if (kernel_data.read == 4)
		{
		  start_decoding (&kernel_data.dc[0]);
		}
	    }
	  else
	    {
	      if (kernel_data.dc[0].max_length <= kernel_data.dc[0].size)
		{
		  r = read (kernel_data.fd,
			    kernel_data.dc[0].buffer + kernel_data.read,
			    kernel_data.dc[0].max_length - kernel_data.read);
		}
	      else
		{
		  int l;

		  l = kernel_data.dc[0].max_length - kernel_data.read;
		  if (l > ZFS_MAXDATA)
		    l = ZFS_MAXDATA;
		  r = read (kernel_data.fd, dummy, l);
		}

	      if (r <= 0)
		break;

	      kernel_data.read += r;
	      if (kernel_data.dc[0].max_length == kernel_data.read)
		{
		  if (kernel_data.dc[0].max_length <= kernel_data.dc[0].size)
		    {
		      DC *dc;

		      zfsd_mutex_lock (&kernel_data.mutex);
		      dc = &kernel_data.dc[0];
		      kernel_data.read = 0;
		      kernel_data.busy++;
		      kernel_data.ndc--;
		      if (kernel_data.ndc > 0)
			kernel_data.dc[0] = kernel_data.dc[kernel_data.ndc];
		      zfsd_mutex_unlock (&kernel_data.mutex);

		      /* We have read complete request, dispatch it.  */
		      kernel_dispatch (dc);
		    }
		  else
		    {
		      message (2, stderr, "Packet too long: %u\n",
			       kernel_data.read);
		      kernel_data.read = 0;
		    }
		}
	    }
	}
    }

  close (kernel_file);
  message (2, stderr, "Terminating...\n");
  return NULL;
}

/* Create a listening socket and start the main kernel thread.  */

bool
kernel_start ()
{
  zfsd_mutex_init (&kernel_data.mutex);

  /* Open connection with kernel.  */
  kernel_file = open (kernel_file_name, O_RDWR);
  if (kernel_file < 0)
    {
      message (-1, stderr, "%s: open(): %s\n", kernel_file_name,
	       strerror (errno));
      return false;
    }

  if (!thread_pool_create (&kernel_pool, 256, 4, 16, kernel_main,
			   kernel_worker, kernel_worker_init))
    {
      close (kernel_file);
      return false;
    }

  return true;
}

/* Terminate kernel threads and destroy data structures.  */

void
kernel_cleanup ()
{
  thread_pool_destroy (&kernel_pool);
  zfsd_mutex_destroy (&kernel_data.mutex);
}
