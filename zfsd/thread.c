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
#include "constant.h"
#include "memory.h"
#include "log.h"
#include "queue.h"
#include "thread.h"
#include "zfs_prot.h"

/* Flag that zfsd is running. It is set to 0 when zfsd is shutting down.  */
int running = 1;

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
  pool->threads = (padded_thread *) xmalloc (max_threads
					     * sizeof (padded_thread));
  queue_create (&pool->idle, max_threads);
  queue_create (&pool->empty, max_threads);

  pthread_mutex_lock (&pool->empty.mutex);
  for (i = 0; i < max_threads; i++)
    {
      pool->threads[i].t.state = THREAD_DEAD;
      pool->threads[i].t.index = i;
      queue_put (&pool->empty, i);
    }
  pthread_mutex_unlock (&pool->empty.mutex);
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

  pthread_mutex_init (&t->mutex, NULL);
  pthread_mutex_lock (&t->mutex);
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
      pthread_mutex_unlock (&t->mutex);
      pthread_mutex_destroy (&t->mutex);
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
  pthread_mutex_unlock (&t->mutex);
  r = pthread_join (t->thread_id, NULL);
  if (r == 0)
    {
      /* Thread left the mutex locked.  */
      pthread_mutex_unlock (&t->mutex);
      pthread_mutex_destroy (&t->mutex);

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

  if (pool->idle.nelem > pool->max_spare_threads)
    {
      /* Let some threads to die.  */
      pthread_mutex_lock (&pool->empty.mutex);
      while (pool->idle.nelem > pool->max_spare_threads)
	{
	  destroy_idle_thread (pool);
	}
      pthread_mutex_unlock (&pool->empty.mutex);
    }
  else if (pool->idle.nelem < pool->max_spare_threads
	   && pool->idle.nelem < pool->idle.size)
    {
      /* Create new threads.  */
      pthread_mutex_lock (&pool->empty.mutex);
      while (pool->idle.nelem < pool->max_spare_threads
	     && pool->idle.nelem < pool->idle.size)
	{
	  create_idle_thread (pool, start, init);
	}
      pthread_mutex_unlock (&pool->empty.mutex);
    }
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
      pthread_mutex_lock (&d->pool->idle.mutex);
      thread_pool_regulate (d->pool, d->start, d->init);
      pthread_mutex_unlock (&d->pool->idle.mutex);
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
