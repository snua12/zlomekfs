/* Functions for managing thread pools.
   Copyright (C) 2003, 2004  Josef Zlomek

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
#include <signal.h>
#include "pthread.h"
#include "constant.h"
#include "semaphore.h"
#include "memory.h"
#include "log.h"
#include "queue.h"
#include "thread.h"

static void *thread_pool_regulator (void *data);

#ifdef ENABLE_CHECKING

/* Static mutex initializer.  */
pthread_mutex_t zfsd_mutex_initializer
  = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

#else

/* Static mutex initializer.  */
pthread_mutex_t zfsd_mutex_initializer
  = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;

#endif

/* Flag that zfsd is running. It is set to 0 when zfsd is shutting down.  */
volatile bool running = true;

/* Mutex protecting RUNNING.  */
pthread_mutex_t running_mutex;

/* Key for thread specific data.  */
pthread_key_t thread_data_key;

/* Get value of RUNNING flag.  */

bool
get_running ()
{
  bool value;

  zfsd_mutex_lock (&running_mutex);
  value = running;
  zfsd_mutex_unlock (&running_mutex);

  return value;
}

/* Set RUNNING flag to VALUE.  */

void
set_running (bool value)
{
  zfsd_mutex_lock (&running_mutex);
  running = value;
  zfsd_mutex_unlock (&running_mutex);
}

/* Terminate blocking syscall in THREAD.  We mark the blocking syscall by
   locking MUTEX.  */

void
thread_terminate_blocking_syscall (pthread_t thid, pthread_mutex_t *mutex)
{
  int i;
  unsigned long delay = 1;

  /* While MUTEX is locked try to terminate syscall.  */
  for (i = 0; i < 3 && pthread_mutex_trylock (mutex) != 0; i++)
    {
      usleep (delay);
      delay *= 500;
      if (pthread_mutex_trylock (mutex) != 0)
	pthread_kill (thid, SIGUSR1);
      else
	break;
    }
  pthread_mutex_unlock (mutex);
}

/* Get state of thread T.  */

thread_state
get_thread_state (thread *t)
{
  thread_state res;

  zfsd_mutex_lock (&t->mutex);
  res = t->state;
  zfsd_mutex_unlock (&t->mutex);

  return res;
}

/* Set state of thread T.  */

void
set_thread_state (thread *t, thread_state state)
{
  zfsd_mutex_lock (&t->mutex);
  t->state = state;
  zfsd_mutex_unlock (&t->mutex);
}

/* Initialize POOL to be a thread pool of MAX_THREADS threads with
   MIN_SPARE (MAX_THREADS) minimum (maximum) number of spare threads.  */

bool
thread_pool_create (thread_pool *pool, size_t max_threads,
		    size_t min_spare_threads, size_t max_spare_threads,
		    thread_start start, thread_initialize init)
{
  size_t i;
  int r;

#ifdef ENABLE_CHECKING
  if (pool->thread_id != 0)
    abort ();
#endif

  pool->min_spare_threads = min_spare_threads;
  pool->max_spare_threads = max_spare_threads;
  pool->size = max_threads;
  pool->unaligned_array = xmalloc (max_threads * sizeof (padded_thread) + 255);
  pool->threads = (padded_thread *) ALIGN_PTR_256 (pool->unaligned_array);
  queue_create (&pool->idle, sizeof (size_t), max_threads);
  queue_create (&pool->empty, sizeof (size_t), max_threads);
  pool->start = start;
  pool->init = init;
  zfsd_mutex_init (&pool->in_syscall);

  zfsd_mutex_lock (&pool->empty.mutex);
  for (i = 0; i < max_threads; i++)
    {
      zfsd_mutex_init (&pool->threads[i].t.mutex);
      set_thread_state (&pool->threads[i].t, THREAD_DEAD);
      pool->threads[i].t.index = i;
      queue_put (&pool->empty, &i);
    }
  zfsd_mutex_unlock (&pool->empty.mutex);

  /* Create worker threads.  */
  zfsd_mutex_lock (&pool->idle.mutex);
  zfsd_mutex_lock (&pool->empty.mutex);
  for (i = 0; i < min_spare_threads; i++)
    {
      r = create_idle_thread (pool);
      if (r != 0)
	{
	  thread_pool_destroy (pool);
	  return false;
	}
    }
  zfsd_mutex_unlock (&pool->empty.mutex);
  zfsd_mutex_unlock (&pool->idle.mutex);

  /* Create thread pool regulator.  */
  r = pthread_create (&pool->thread_id, NULL, thread_pool_regulator,
		      (void *) pool);
  if (r != 0)
    {
      message (-1, stderr, "pthread_create() failed\n");
      thread_pool_destroy (pool);
      return false;
    }

  return true;
}

/* Destroy thread pool POOL - terminate idle threads, wait for active threads to
   finish, free memory associated with thread pool.  */

void
thread_pool_destroy (thread_pool *pool)
{
  size_t i;

  zfsd_mutex_lock (&pool->idle.mutex);
  zfsd_mutex_lock (&pool->empty.mutex);

  while (pool->empty.nelem < pool->size)
    destroy_idle_thread (pool);

  zfsd_mutex_unlock (&pool->empty.mutex);
  zfsd_mutex_unlock (&pool->idle.mutex);

  /* Some thread may have these mutexes locked, wait for it to unlock them.  */
  zfsd_mutex_lock (&pool->idle.mutex);
  zfsd_mutex_lock (&pool->empty.mutex);

  for (i = 0; i < pool->size; i++)
    {
      zfsd_mutex_destroy (&pool->threads[i].t.mutex);
    }
  free (pool->unaligned_array);
  queue_destroy (&pool->empty);
  queue_destroy (&pool->idle);
}

/* Create a new idle thread in thread pool POOL.
   This function expects NETWORK_POOL.EMPTY.MUTEX and NETWORK_POOL.IDLE.MUTEX
   to be locked.  */

int
create_idle_thread (thread_pool *pool)
{
  size_t index;
  thread *t;
  int r;

  CHECK_MUTEX_LOCKED (&pool->idle.mutex);
  CHECK_MUTEX_LOCKED (&pool->empty.mutex);

  queue_get (&pool->empty, &index);
  t = &pool->threads[index].t;

  r = semaphore_init (&t->sem, 0);
  if (r != 0)
    {
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, &index);
      message (-1, stderr, "semaphore_init() failed\n");
      return r;
    }

  t->state = THREAD_IDLE;
  r = pthread_create (&t->thread_id, NULL, pool->start, t);
  if (r == 0)
    {
      /* Call the initializer before we put the thread to the idle queue.  */
      if (pool->init)
	(*pool->init) (t);

      queue_put (&pool->idle, &index);
    }
  else
    {
      semaphore_destroy (&t->sem);
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, &index);
      message (-1, stderr, "pthread_create() failed\n");
    }

  return r;
}

/* Destroy an idle thread in thread pool POOL.  This function expects
   NETWORK_POOL.EMPTY.MUTEX and NETWORK_POOL.IDLE.MUTEX to be locked.  */

int
destroy_idle_thread (thread_pool *pool)
{
  size_t index;
  thread *t;
  int r;

  CHECK_MUTEX_LOCKED (&pool->idle.mutex);
  CHECK_MUTEX_LOCKED (&pool->empty.mutex);

  queue_get (&pool->idle, &index);
  t = &pool->threads[index].t;

  set_thread_state (t, THREAD_DYING);
  semaphore_up (&t->sem, 1);
  r = pthread_join (t->thread_id, NULL);
  if (r == 0)
    {
      semaphore_destroy (&t->sem);
      set_thread_state (t, THREAD_DEAD);
      queue_put (&pool->empty, &index);
    }
  else
    {
      message (-1, stderr, "pthread_join() failed\n");
    }

  return r;
}

/* Disable receiving signals by calling thread.  */

void
thread_disable_signals ()
{
  sigset_t mask;

  sigemptyset (&mask);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, SIGQUIT);
  sigaddset (&mask, SIGTERM);
  pthread_sigmask (SIG_BLOCK, &mask, NULL);
}

/* Kill/create threads when there are too many or not enough idle threads.
   It expects NETWORK_POOL.IDLE.MUTEX to be locked.  */

void
thread_pool_regulate (thread_pool *pool)
{
  CHECK_MUTEX_LOCKED (&pool->idle.mutex);

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
      create_idle_thread (pool);
    }

  zfsd_mutex_unlock (&pool->empty.mutex);
}

/* Main function of thread regulating the thread pool. DATA is the structure
   with additional information for the regulator.  */

static void *
thread_pool_regulator (void *data)
{
  thread_pool *pool = (thread_pool *) data;

  thread_disable_signals ();

  while (get_running ())
    {
      zfsd_mutex_lock (&pool->in_syscall);
      if (get_running ())
	sleep (THREAD_POOL_REGULATOR_INTERVAL);
      zfsd_mutex_unlock (&pool->in_syscall);
      if (!get_running ())
	break;
      zfsd_mutex_lock (&pool->idle.mutex);
      thread_pool_regulate (pool);
      zfsd_mutex_unlock (&pool->idle.mutex);
    }

  /* Disable signaling this thread. */
  zfsd_mutex_lock (&running_mutex);
  pool->thread_id = 0;
  zfsd_mutex_unlock (&running_mutex);

  zfsd_mutex_destroy (&pool->in_syscall);
  return NULL;
}
