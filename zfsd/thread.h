/* Functions for managing thread pools.
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

#ifndef THREAD_H
#define THREAD_H

#include "system.h"
#include <stddef.h>
#include <pthread.h>
#include <rpc/rpc.h>
#include "queue.h"
#include "server.h"
#include "zfs_prot.h"

/* Flag that zfsd is running. It is set to 0 when zfsd is shutting down.  */
extern volatile int running;

/* State of the thread.  */
typedef enum thread_state_def
{
  THREAD_DEAD,		/* thread is not created */
  THREAD_DYING,		/* thread is dying */
  THREAD_IDLE,		/* thread is idle */
  THREAD_BUSY		/* thread is working */
} thread_state;

/* Additional data for a client thread.  */
typedef struct client_thread_data_def
{
  /* Buffer for data.  */
  char *buffer;
} client_thread_data;

/* Definition of thread's variables.  */
typedef struct thread_def
{
  /* State of the thread.  */
  thread_state state;

  /* The sequential number of the thread.  */
  size_t index;

  /* The ID of the thread which is set by pthread_create.  */
  pthread_t thread_id;

  /* Mutex used to stop an idle thread.  */
  pthread_mutex_t mutex;

  /* Additional data for each subtype.  */
  union {
    server_thread_data server;
    client_thread_data client;
#if 0
    update_thread_data update;
#endif
  } u;
} thread;

/* Thread datatype padded to 256 bytes to avoid cache ping pong.  */
typedef union padded_thread_def
{
  thread t;
  char padding[((sizeof (thread) + 255) / 256) * 256];
} padded_thread;

/* Definition of thread pool.  */
typedef struct thread_pool_def
{
  size_t min_spare_threads;	/* minimal number of spare threads */
  size_t max_spare_threads;	/* maximal number of spare threads */
  size_t size;			/* total number of slots for threads */
  void *unaligned_array;	/* pointer returned by xmalloc */
  padded_thread *threads;	/* thread slots, previous pointer aligned */
  queue idle;			/* queue of idle threads */
  queue empty;			/* queue of empty thread slots */
} thread_pool;

/* Description of thread waiting for reply.  */
typedef struct waiting4reply_data_def
{
  unsigned int request_id;
  thread *t;
} waiting4reply_data;

/* Type of a routine started in a new thread.  */
typedef void *(*thread_start) (void *);

/* Type of thread initializer.  */
typedef void (*thread_initialize) (thread *);

/* Data for thread_pool_regulator.  */
typedef struct thread_pool_regulator_data_def
{
  pthread_t thread_id;		/* thread ID of the regulator */
  thread_pool *pool;		/* thread pool which the regulator regulates */
  thread_start start;		/* start routine of the worker thread */
  thread_initialize init;	/* initialization routine */
} thread_pool_regulator_data;

extern void thread_pool_create (thread_pool *pool, size_t max_threads,
				size_t min_spare_threads,
				size_t max_spare_threads);
extern int create_idle_thread (thread_pool *pool, thread_start start,
			       thread_initialize init);
extern int destroy_idle_thread (thread_pool *pool);
extern void thread_pool_regulate (thread_pool *pool, thread_start start,
				  thread_initialize init);
extern void thread_pool_create_regulator (thread_pool_regulator_data *data,
					  thread_pool *pool,
					  thread_start start,
					  thread_initialize init);

#endif
