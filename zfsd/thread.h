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
#include "queue.h"
#include "zfs_prot.h"

/* State of the thread.  */
typedef enum thread_state_def
{
  THREAD_NONE,
  THREAD_IDLE,
  THREAD_BUSY
} thread_state;

/* Additional data for a server thread.  */
typedef struct server_thread_data_def
{
  /* Parameters passed to zfs_program_1 */
  struct svc_req *rqstp;
  SVCXPRT *transp;
} server_thread_data;

/* Definition of thread's variables.  */
typedef struct thread_def
{
  /* State of the thread.  */
  thread_state state;

  /* Mutex used to stop an idle thread.  */
  pthread_mutex_t mutex;

  /* Additional data for each subtype.  */
  union {
    server_thread_data server;
#if 0
    client_thread_data client;
    update_thread_data update;
#endif
  };
} thread;

/* Thread datatype padded to a multiple of 32 to avoid cache ping pong.  */
typedef union padded_thread_def
{
  thread t;
  char padding[64];
} padded_thread;

/* Definition of thread pool.  */
typedef struct thread_pool_def {
  size_t nthreads;		/* total number of threads */
  size_t min_spare_threads;	/* minimal number of spare threads */
  size_t max_spare_threads;	/* maximal number of spare threads */
  size_t size;			/* total number of slots for threads */
  padded_thread *threads;	/* thread slots */
  queue idle;			/* queue of idle threads */
  queue empty;			/* queue of empty thread slots */
} thread_pool;

extern void thread_pool_create (thread_pool *pool, size_t max_threads,
				size_t min_spare_threads,
				size_t max_spare_threads);
#endif
