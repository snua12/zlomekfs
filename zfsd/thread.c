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

#include "system.h"
#include <stddef.h>
#include <unistd.h>
#include "pthread.h"
#include "constant.h"
#include "semaphore.h"
#include "memory.h"
#include "log.h"
#include "queue.h"
#include "thread.h"

/* Flag that zfsd is running. It is set to 0 when zfsd is shutting down.  */
volatile int running = 1;

/* Key for server thread specific data.  */
pthread_key_t server_thread_key;

/* Initialize POOL to be a thread pool of MAX_THREADS threads with
   MIN_SPARE (MAX_THREADS) minimum (maximum) number of spare threads.  */

void
thread_pool_create (thread_pool *pool, size_t max_threads,
		    size_t min_spare_threads, size_t max_spare_threads)
{
  size_t i;

  pool->min_spare_threads = min_spare_threads;
  pool->max_spare_threads = max_spare_threads;
  pool->size = max_threads;
  pool->unaligned_array = xmalloc (max_threads * sizeof (padded_thread) + 255);
  pool->threads = (padded_thread *) ALIGN_PTR_256 (pool->unaligned_array);
  queue_create (&pool->idle, max_threads);
  queue_create (&pool->empty, max_threads);

  zfsd_mutex_lock (&pool->empty.mutex);
  for (i = 0; i < max_threads; i++)
    {
      pool->threads[i].t.state = THREAD_DEAD;
      pool->threads[i].t.index = i;
      queue_put (&pool->empty, i);
    }
  zfsd_mutex_unlock (&pool->empty.mutex);
}

/* Destroy thread pool POOL - terminate idle threads, wait for active threads to
   finish, free memory associated with thread pool.  */

void
thread_pool_destroy (thread_pool *pool)
{
  zfsd_mutex_lock (&pool->idle.mutex);
  zfsd_mutex_lock (&pool->empty.mutex);

  while (pool->empty.nelem < pool->size)
    destroy_idle_thread (pool);

  zfsd_mutex_unlock (&pool->empty.mutex);
  zfsd_mutex_unlock (&pool->idle.mutex);

  free (pool->unaligned_array);
  queue_destroy (&pool->empty);
  queue_destroy (&pool->idle);
}

/* Create a new idle thread in thread pool POOL and start a routine START in it.
   This function expects SERVER_POLL.EMPTY.MUTEX and SERVER_POOL.IDLE.MUTEX
   to be locked.  */

int
create_idle_thread (thread_pool *pool, thread_start start,
		    thread_initialize init)
{
  size_t index = queue_get (&pool->empty);
  thread *t = &pool->threads[index].t;
  int r;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&pool->idle.mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&pool->empty.mutex) == 0)
    abort ();
#endif

  r = semaphore_init (&t->sem, 0);
  if (r != 0)
    {
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, index);
      message (-1, stderr, "semaphore_init() failed\n");
      return r;
    }

  t->state = THREAD_IDLE;
  r = pthread_create (&t->thread_id, NULL, start, t);
  if (r == 0)
    {
      /* Call the initializer before we put the thread to the idle queue.  */
      if (init)
	(*init) (t);

      queue_put (&pool->idle, index);
    }
  else
    {
      semaphore_destroy (&t->sem);
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, index);
      message (-1, stderr, "pthread_create() failed\n");
    }

  return r;
}

/* Destroy an idle thread in thread pool POOL.  This function expects
   SERVER_POLL.EMPTY.MUTEX and SERVER_POOL.IDLE.MUTEX to be locked.  */

int
destroy_idle_thread (thread_pool *pool)
{
  size_t index = queue_get (&pool->idle);
  thread *t = &pool->threads[index].t;
  int r;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&pool->idle.mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&pool->empty.mutex) == 0)
    abort ();
#endif

  t->state = THREAD_DYING;
  semaphore_up (&t->sem, 1);
  r = pthread_join (t->thread_id, NULL);
  if (r == 0)
    {
      semaphore_destroy (&t->sem);
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, index);
    }
  else
    {
      message (-1, stderr, "pthread_join() failed\n");
    }

  return r;
}

/* Kill/create threads when there are too many or not enough idle threads.
   It expects SERVER_POOL.IDLE.MUTEX to be locked.  */

void
thread_pool_regulate (thread_pool *pool, thread_start start,
		      thread_initialize init)
{
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&pool->idle.mutex) == 0)
    abort ();
#endif

  zfsd_mutex_lock (&pool->empty.mutex);

  /* Let some threads to die.  */
  while (pool->idle.nelem > pool->max_spare_threads)
    {
      message (2, stderr, "Regulating: destroying idle thread\n");
      destroy_idle_thread (pool);
    }

  /* Create new threads.  */
  while (pool->idle.nelem < pool->min_spare_threads
	 && pool->idle.nelem < pool->idle.size)
    {
      message (2, stderr, "Regulating: creating idle thread\n");
      create_idle_thread (pool, start, init);
    }

  zfsd_mutex_unlock (&pool->empty.mutex);
}

/* Main function of thread regulating the thread pool. DATA is the structure
   with additional information for the regulator.  */

static void *
thread_pool_regulator (void *data)
{
  thread_pool_regulator_data *d = (thread_pool_regulator_data *) data;

  while (running)
    {
      sleep (THREAD_POOL_REGULATOR_INTERVAL);
      if (!running)
	return NULL;
      zfsd_mutex_lock (&d->pool->idle.mutex);
      thread_pool_regulate (d->pool, d->start, d->init);
      zfsd_mutex_unlock (&d->pool->idle.mutex);
    }

  return NULL;
}

/* Create a thread regulating the thread pool POOL which uses START to start
   a new thread and INIT to initialize thread's data. DATA is the structure with
   additional information for the regulator.  */

void
thread_pool_create_regulator (thread_pool_regulator_data *data,
			      thread_pool *pool, thread_start start,
			      thread_initialize init)
{
  data->pool = pool;
  data->start = start;
  data->init = init;
  if (pthread_create (&data->thread_id, NULL, thread_pool_regulator,
		      (void *) data))
    {
      message (-1, stderr, "pthread_create() failed\n");
    }
}
