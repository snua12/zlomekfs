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
#include "network.h"
#include "log.h"
#include "node.h"
#include "util.h"
#include "memory.h"
#include "thread.h"
#include "zfs_prot.h"
#include "config.h"
#include "fh.h"

/* Pool of kernel threads (threads communicating with kernel).  */
thread_pool kernel_pool;

/* File descriptor of file communicating with kernel.  */
int kernel_fd = -1;

/* Is ZFS mounted?  */
bool mounted = false;

/* Initialize data for kernel file descriptor.  */

static void
init_fd_data (void)
{
#ifdef ENABLE_CHECKING
  if (kernel_fd < 0)
    abort ();
#endif

  zfsd_mutex_lock (&fd_data_a[kernel_fd].mutex);
  fd_data_a[kernel_fd].fd = kernel_fd;
  fd_data_a[kernel_fd].read = 0;
  fd_data_a[kernel_fd].busy = 0;
  if (fd_data_a[kernel_fd].ndc == 0)
    {
      fd_data_a[kernel_fd].dc[0] = dc_create ();
      fd_data_a[kernel_fd].ndc++;
    }

  fd_data_a[kernel_fd].waiting4reply_pool
    = create_alloc_pool ("waiting4reply_data",
			 sizeof (waiting4reply_data), 30,
			 &fd_data_a[kernel_fd].mutex);
  fd_data_a[kernel_fd].waiting4reply_heap
    = fibheap_new (30, &fd_data_a[kernel_fd].mutex);
  fd_data_a[kernel_fd].waiting4reply
    = htab_create (30, waiting4reply_hash, waiting4reply_eq,
		   NULL, &fd_data_a[kernel_fd].mutex);
  zfsd_mutex_unlock (&fd_data_a[kernel_fd].mutex);
}

/* Close kernel file and destroy data structures used by it.  */

void
close_kernel_fd (void)
{
  int j;

  if (kernel_fd < 0)
    return;

  close (kernel_fd);
  zfsd_mutex_lock (&fd_data_a[kernel_fd].mutex);
  wake_all_threads (&fd_data_a[kernel_fd], ZFS_CONNECTION_CLOSED);
  for (j = 0; j < fd_data_a[kernel_fd].ndc; j++)
    dc_destroy (fd_data_a[kernel_fd].dc[j]);

  htab_destroy (fd_data_a[kernel_fd].waiting4reply);
  fibheap_delete (fd_data_a[kernel_fd].waiting4reply_heap);
  free_alloc_pool (fd_data_a[kernel_fd].waiting4reply_pool);
  zfsd_mutex_unlock (&fd_data_a[kernel_fd].mutex);

  kernel_fd = -1;
}

/* Send a reply.  */

static void
send_reply (thread *t)
{
  message (2, stderr, "sending reply\n");
  zfsd_mutex_lock (&fd_data_a[kernel_fd].mutex);
  if (!full_write (kernel_fd, t->u.kernel.dc->buffer,
		   t->u.kernel.dc->cur_length))
    {
    }
  zfsd_mutex_unlock (&fd_data_a[kernel_fd].mutex);
}

/* Send error reply with error status STATUS.  */

static void
send_error_reply (thread *t, uint32_t request_id, int32_t status)
{
  start_encoding (t->u.kernel.dc);
  encode_direction (t->u.kernel.dc, DIR_REPLY);
  encode_request_id (t->u.kernel.dc, request_id);
  encode_status (t->u.kernel.dc, status);
  finish_encoding (t->u.kernel.dc);
  send_reply (t);
}

/* Initialize kernel thread T.  */

static void
kernel_worker_init (thread *t)
{
  t->dc_call = dc_create ();
}

/* Cleanup kernel thread DATA.  */

static void
kernel_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (t->dc_call);
}

/* The main function of the kernel thread.  */

static void *
kernel_worker (void *data)
{
  thread *t = (thread *) data;
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  uint32_t request_id;
  uint32_t fn;

  thread_disable_signals ();

  pthread_cleanup_push (kernel_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Kernel worker thread");
  set_lock_info (li);

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

      if (!decode_request_id (t->u.kernel.dc, &request_id))
	{
	  /* TODO: log too short packet.  */
	  goto out;
	}

      if (t->u.kernel.dc->max_length > DC_SIZE)
	{
	  send_error_reply (t, request_id, ZFS_REQUEST_TOO_LONG);
	  goto out;
	}

      if (!decode_function (t->u.kernel.dc, &fn))
	{
	  send_error_reply (t, request_id, ZFS_INVALID_REQUEST);
	  goto out;
	}

      /* ZFS is mounted if kernel wants something from zfsd.  */
      mounted = true;

      message (2, stderr, "REQUEST: ID=%u function=%u\n", request_id, fn);
      switch (fn)
	{
#define ZFS_CALL_SERVER
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
	  case ZFS_PROC_##NAME:						\
	    if (!decode_##ARGS (t->u.kernel.dc,				\
				&t->u.kernel.args.FUNCTION)		\
		|| !finish_decoding (t->u.kernel.dc))			\
	      {								\
		send_error_reply (t, request_id, ZFS_INVALID_REQUEST);	\
		goto out;						\
	      }								\
	    call_statistics[CALL_FROM_KERNEL][NUMBER]++;		\
	    start_encoding (t->u.kernel.dc);				\
	    encode_direction (t->u.kernel.dc, DIR_REPLY);		\
	    encode_request_id (t->u.kernel.dc, request_id);		\
	    zfs_proc_##FUNCTION##_server (&t->u.kernel.args.FUNCTION,	\
					  t->u.kernel.dc,		\
					  &t->u.kernel, true);		\
	    finish_encoding (t->u.kernel.dc);				\
	    send_reply (t);						\
	    break;
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_SERVER

	  default:
	    send_error_reply (t, request_id, ZFS_UNKNOWN_FUNCTION);
	    goto out;
	}

out:
      zfsd_mutex_lock (&fd_data_a[kernel_fd].mutex);
      fd_data_a[kernel_fd].busy--;
      recycle_dc_to_fd_data (t->u.kernel.dc, &fd_data_a[kernel_fd]);
      zfsd_mutex_unlock (&fd_data_a[kernel_fd].mutex);

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

static bool
kernel_dispatch (fd_data_t *fd_data)
{
  DC *dc = fd_data_a[kernel_fd].dc[0];
  size_t index;
  direction dir;

  CHECK_MUTEX_LOCKED (&fd_data_a[kernel_fd].mutex);

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
		message (1, stderr, "Packet from kernel too short.\n");
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
	    t->dc_reply = dc;
	    htab_clear_slot (fd_data->waiting4reply, slot);
	    fibheap_delete_node (fd_data->waiting4reply_heap, data->node);
	    pool_free (fd_data->waiting4reply_pool, data);

	    /* Let the thread run again.  */
	    semaphore_up (&t->sem, 1);
	  }
	break;

      case DIR_REQUEST:
	/* Dispatch request.  */
	fd_data->busy++;

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
	kernel_pool.threads[index].t.u.kernel.dc = dc;
	network_pool.threads[index].t.u.network.fd_data = fd_data;

	/* Let the thread run.  */
	semaphore_up (&kernel_pool.threads[index].t.sem, 1);

	zfsd_mutex_unlock (&kernel_pool.idle.mutex);
	break;

      default:
	/* This case never happens, it is caught in the beginning of this
	   function. It is here to make compiler happy.  */
	abort ();
    }

  return true;
}

/* Main function of the main (i.e. listening) kernel thread.  */

static void *
kernel_main (ATTRIBUTE_UNUSED void *data)
{
  ssize_t r;
  fd_data_t *fd_data;

  thread_disable_signals ();
  pthread_setspecific (thread_name_key, "Kernel main thread");

  fd_data = &fd_data_a[kernel_fd];
  while (!thread_pool_terminate_p (&kernel_pool))
    {
      zfsd_mutex_lock (&fd_data->mutex);
      if (fd_data->ndc == 0)
	{
	  fd_data->dc[0] = dc_create ();
	  fd_data->ndc++;
	}
      zfsd_mutex_unlock (&fd_data->mutex);

      zfsd_mutex_lock (&kernel_pool.main_in_syscall);
      r = read (kernel_fd, fd_data->dc[0]->buffer, DC_SIZE);
      zfsd_mutex_unlock (&kernel_pool.main_in_syscall);

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

      start_decoding (fd_data->dc[0]);

      if (fd_data->dc[0]->max_length != (unsigned int) r)
	{
	  message (1, stderr, "Invalid packet from kernel.\n");

	  /* If the packet length is at least 12 we will decode the request_id.
	     So if the packet length is smaller than DC_SIZE
	     we can send a ZFS_INVALID_REQUEST error reply
	     or return a ZFS_INVALID_REPLY error to caller thread.
	     If the packet is larger than DC_SIZE we will
	     return a ZFS_REPLY_TOO_LONG error.  */
	  if (fd_data->dc[0]->max_length >= 12
	      && fd_data->dc[0]->max_length <= DC_SIZE)
	    fd_data->dc[0]->max_length = 12;
	}

      /* Dispatch the packet.  */
      zfsd_mutex_lock (&fd_data->mutex);
      if (kernel_dispatch (fd_data))
	{
	  fd_data->ndc--;
	  if (fd_data->ndc > 0)
	    fd_data->dc[0] = fd_data->dc[fd_data->ndc];
	}
      zfsd_mutex_unlock (&fd_data->mutex);
    }

  message (2, stderr, "Terminating...\n");
  return NULL;
}

/* Create a listening socket and start the main kernel thread.  */

bool
kernel_start (void)
{
  /* Open connection with kernel.  */
  kernel_fd = open (kernel_file_name, O_RDWR);
  if (kernel_fd < 0)
    {
      message (-1, stderr, "%s: open(): %s\n", kernel_file_name,
	       strerror (errno));
      return false;
    }

  init_fd_data ();

  if (!thread_pool_create (&kernel_pool, 256, 4, 16, kernel_main,
			   kernel_worker, kernel_worker_init))
    {
      close_kernel_fd ();
      return false;
    }

  return true;
}

/* Terminate kernel threads and destroy data structures.  */

void
kernel_cleanup (void)
{
  thread_pool_destroy (&kernel_pool);
}
