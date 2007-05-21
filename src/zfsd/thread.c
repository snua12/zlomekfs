/*! \file
    \brief Functions for managing thread pools.  */

/* Copyright (C) 2003, 2004  Josef Zlomek

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
#include <errno.h>
#include <signal.h>
#include "pthread.h"
#include "constant.h"
#include "semaphore.h"
#include "memory.h"
#include "log.h"
#include "queue.h"
#include "thread.h"

static void *thread_pool_regulator (void *data);

/*! Flag that zfsd is running. It is set to 0 when zfsd is shutting down.  */
volatile bool running = true;

/*! Mutex protecting RUNNING.  */
pthread_mutex_t running_mutex;

/*! Key for thread specific data.  */
pthread_key_t thread_data_key;

/*! Key for thread name.  */
pthread_key_t thread_name_key;

/*! Limits for number of network threads.  */
thread_limit network_thread_limit = {8, 2, 4};

/*! Limits for number of kernel threads.  */
thread_limit kernel_thread_limit = {4, 1, 2};

/*! Limits for number of update threads.  */
thread_limit update_thread_limit = {4, 1, 2};

/*! Get value of RUNNING flag.  */

bool
get_running (void)
{
  bool value;

  zfsd_mutex_lock (&running_mutex);
  value = running;
  zfsd_mutex_unlock (&running_mutex);

  return value;
}

/*! Shall the worker threads terminate?  */

bool
thread_pool_terminate_p (thread_pool *pool)
{
  bool value;

  zfsd_mutex_lock (&running_mutex);
  value = pool->terminate;
  zfsd_mutex_unlock (&running_mutex);

  return value;
}

/*! Terminate blocking syscall in thread *THID.  We mark the blocking syscall by
   locking MUTEX.  */
//NOTE: interresting heuristic
void
thread_terminate_blocking_syscall (volatile pthread_t *thid,
				   pthread_mutex_t *mutex)
{
  int i;
  unsigned long delay = 1;

  zfsd_mutex_lock (&running_mutex);

  if (*thid == 0)
    {
      zfsd_mutex_unlock (&running_mutex);
      return;
    }

  /* While MUTEX is locked try to terminate syscall.  */
  for (i = 0; i < 3 && pthread_mutex_trylock (mutex) != 0; i++)
    {
      zfsd_mutex_unlock (&running_mutex);
      usleep (delay);
      zfsd_mutex_lock (&running_mutex);

      if (*thid == 0)
	{
	  zfsd_mutex_unlock (&running_mutex);
	  return;
	}

      delay *= 500;
      if (pthread_mutex_trylock (mutex) != 0)
	{
	  message (LOG_INFO, NULL, "killing %lu\n", *thid); //NOTE: try to track this with unexpected manners
	  pthread_kill (*thid, SIGUSR1);
	}
      else
	break;
    }
  pthread_mutex_unlock (mutex);

  zfsd_mutex_unlock (&running_mutex);
}

/*! Wait for thread *THID to die and store its return value to RET.  */

int
wait_for_thread_to_die (volatile pthread_t *thid, void **ret)
{
  pthread_t id;
  int r;

  zfsd_mutex_lock (&running_mutex);
  id = *thid;
  zfsd_mutex_unlock (&running_mutex);

  if (id == 0)
    return ESRCH;

  message (LOG_DEBUG, NULL, "joining %lu\n", id);
  r = pthread_join (id, ret);
  if (r == 0)
    message (LOG_DEBUG, NULL, "joined %lu\n", id);

  /* Disable destroying this thread.  */ //NOTE: this is o.k., but what about side efffects?
  zfsd_mutex_lock (&running_mutex);
  *thid = 0;
  zfsd_mutex_unlock (&running_mutex);

  return r;
}

/*! Get state of thread T.  */

thread_state
get_thread_state (thread *t)
{
  thread_state res;

  zfsd_mutex_lock (&t->mutex);
  res = t->state;
  zfsd_mutex_unlock (&t->mutex);

  return res;
}

/*! Set state of thread T.  */

void
set_thread_state (thread *t, thread_state state)
{
  zfsd_mutex_lock (&t->mutex);
  t->state = state;
  zfsd_mutex_unlock (&t->mutex);
}

/*! Initialize the thread pool.
    \param pool The thread pool to initialize.
    \param limit Limits for number of threads.
    \param main_start Start routine of the main thread of the pool.
    \param worker_start Start routine of the worker thread of the pool.
    \param worker_init Initialization of the worker thread.  */

bool
thread_pool_create (thread_pool *pool, thread_limit *limit,
		    thread_start main_start,
		    thread_start worker_start, thread_init worker_init)
{
  size_t i;
  int r;

#ifdef ENABLE_CHECKING
  if (pool->main_thread != 0)
    abort ();
  if (pool->regulator_thread != 0)
    abort ();
#endif

  pool->terminate = !get_running ();
  if (pool->terminate)
    return false;

  pool->min_spare_threads = limit->min_spare;
  pool->max_spare_threads = limit->max_spare;
  pool->size = limit->max_total;
  pool->unaligned_array = xmalloc (pool->size * sizeof (padded_thread) + 255);
  pool->threads = (padded_thread *) ALIGN_PTR_256 (pool->unaligned_array);
  zfsd_mutex_init (&pool->mutex);
  queue_create (&pool->idle, sizeof (size_t), pool->size, &pool->mutex);
  queue_create (&pool->empty, sizeof (size_t), pool->size, &pool->mutex);
  pool->worker_start = worker_start;
  pool->worker_init = worker_init;
  zfsd_mutex_init (&pool->main_in_syscall);
  zfsd_mutex_init (&pool->regulator_in_syscall);

  zfsd_mutex_lock (&pool->mutex);
  for (i = 0; i < pool->size; i++)
    {
      zfsd_mutex_init (&pool->threads[i].t.mutex);
      set_thread_state (&pool->threads[i].t, THREAD_DEAD);
      pool->threads[i].t.index = i;
      queue_put (&pool->empty, &i);
    }
  zfsd_mutex_unlock (&pool->mutex);
//NOTE: why thiw unlock/lock gap?
  /* Create worker threads.  */
  zfsd_mutex_lock (&pool->mutex);
  for (i = 0; i < pool->min_spare_threads; i++)
    {
      r = create_idle_thread (pool);
      if (r != 0)
	{
	  thread_pool_destroy (pool);
	  return false;
	}
    }
  zfsd_mutex_unlock (&pool->mutex);
//NOTE: ^ regulator & main thread init may not (should not)  be locked?
  /* Create thread pool regulator.  */
  r = pthread_create (CAST_QUAL (pthread_t *, &pool->regulator_thread), NULL,
		      thread_pool_regulator, pool);
  if (r != 0)
    {
      message (LOG_ERROR, stderr, "pthread_create() failed\n");
      thread_pool_destroy (pool);
      return false;
    }

  /* Create main thread pool.  */
  r = pthread_create (CAST_QUAL (pthread_t *, &pool->main_thread), NULL,
		      main_start, pool);
  if (r != 0)
    {
      message (LOG_ERROR, stderr, "pthread_create() failed\n");
      thread_pool_terminate (pool);
      thread_pool_destroy (pool);
      return false;
    }

  return true;
}

/*! Terminate the main and regulator threads in thread pool POOL
   and tell worker threads to finish.  */

void
thread_pool_terminate (thread_pool *pool)
{
  zfsd_mutex_lock (&running_mutex);
  pool->terminate = true;	/* used in main thread to finish */
  if (pool->main_thread != 0)
    {
      zfsd_mutex_unlock (&running_mutex);
      queue_exiting (&pool->idle);
      queue_exiting (&pool->empty);
    }
  else
    zfsd_mutex_unlock (&running_mutex);

  thread_terminate_blocking_syscall (&pool->main_thread,
				     &pool->main_in_syscall);
  thread_terminate_blocking_syscall (&pool->regulator_thread,
				     &pool->regulator_in_syscall);
}

/*! Destroy thread pool POOL - terminate idle threads, wait for active threads to
   finish, free memory associated with thread pool.  */

void
thread_pool_destroy (thread_pool *pool)
{
  size_t i;

  pthread_yield ();
  wait_for_thread_to_die (&pool->main_thread, NULL);
  wait_for_thread_to_die (&pool->regulator_thread, NULL);
  zfsd_mutex_destroy (&pool->main_in_syscall);
  zfsd_mutex_destroy (&pool->regulator_in_syscall);

  /* Wait until all worker threads are idle and destroy them.  */
  zfsd_mutex_lock (&pool->mutex);
  while (pool->empty.nelem < pool->size)
    destroy_idle_thread (pool);
  zfsd_mutex_unlock (&pool->mutex);

  /* Some thread may have these mutexes locked, wait for it to unlock them.  */
  zfsd_mutex_lock (&pool->mutex);
  for (i = 0; i < pool->size; i++)
    zfsd_mutex_destroy (&pool->threads[i].t.mutex);
  free (pool->unaligned_array);
  queue_destroy (&pool->empty);
  queue_destroy (&pool->idle);
  zfsd_mutex_unlock (&pool->mutex);
  zfsd_mutex_destroy (&pool->mutex);
}

/*! Create a new idle thread in thread pool POOL.
   This function expects NETWORK_POOL.EMPTY.MUTEX and NETWORK_POOL.IDLE.MUTEX
   to be locked.  */

int
create_idle_thread (thread_pool *pool)
{
  size_t idx;
  thread *t;
  int r;

  CHECK_MUTEX_LOCKED (&pool->mutex);//NOTE: why makro????

  queue_get (&pool->empty, &idx);
  t = &pool->threads[idx].t;

  r = semaphore_init (&t->sem, 0);
  if (r != 0)
    {
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, &idx);
      message (LOG_ERROR, stderr, "semaphore_init() failed\n");
      return r;
    }

  t->state = THREAD_IDLE;
  r = pthread_create (&t->thread_id, NULL, pool->worker_start, t);
  if (r == 0)
    {
      /* Call the initializer before we put the thread to the idle queue.  */
      if (pool->worker_init)
	(*pool->worker_init) (t);

      queue_put (&pool->idle, &idx);
    }
  else
    {
      semaphore_destroy (&t->sem);
      t->state = THREAD_DEAD;
      queue_put (&pool->empty, &idx);
      message (LOG_ERROR, stderr, "pthread_create() failed\n");
    }

  return r;
}

/*! Destroy an idle thread in thread pool POOL.  This function expects
   NETWORK_POOL.EMPTY.MUTEX and NETWORK_POOL.IDLE.MUTEX to be locked.  */

int
destroy_idle_thread (thread_pool *pool)
{
  size_t idx;
  thread *t;
  int r;

  CHECK_MUTEX_LOCKED (&pool->mutex);

  /* Let the thread which was busy add itself to idle queue.  */
  zfsd_mutex_unlock (&pool->mutex);
  zfsd_mutex_lock (&pool->mutex);

  queue_get (&pool->idle, &idx);
  t = &pool->threads[idx].t;

  set_thread_state (t, THREAD_DYING);
  semaphore_up (&t->sem, 1);
  r = pthread_join (t->thread_id, NULL);
  if (r == 0)
    {
      semaphore_destroy (&t->sem);
      set_thread_state (t, THREAD_DEAD);
      queue_put (&pool->empty, &idx);
    }
  else
    {
      message (LOG_ERROR, stderr, "pthread_join() failed\n");
    }

  return r;
}

/*! Disable receiving signals by calling thread.  */

void
thread_disable_signals (void)
{
  sigset_t mask;

  sigemptyset (&mask);
  sigaddset (&mask, SIGHUP);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, SIGQUIT);
  sigaddset (&mask, SIGTERM);
  pthread_sigmask (SIG_BLOCK, &mask, NULL);
}

/*! Kill/create threads when there are too many or not enough idle threads.
   It expects NETWORK_POOL.IDLE.MUTEX to be locked.  */

void
thread_pool_regulate (thread_pool *pool)
{
  CHECK_MUTEX_LOCKED (&pool->mutex);

  /* Let some threads to die.  */
  while (pool->idle.nelem > pool->max_spare_threads)
    {
      message (LOG_INFO, NULL, "Regulating: destroying idle thread\n");
      destroy_idle_thread (pool);
    }

  /* Create new threads.  */
  while (pool->idle.nelem < pool->min_spare_threads
	 && pool->idle.nelem < pool->idle.size
	 && pool->empty.nelem > 0)
    {
      message (LOG_INFO, NULL, "Regulating: creating idle thread\n");
      create_idle_thread (pool);
    }
}

/*! Main function of thread regulating the thread pool. DATA is the structure
   with additional information for the regulator.  */

static void *
thread_pool_regulator (void *data)
{
  thread_pool *pool = (thread_pool *) data;

  thread_disable_signals ();
  pthread_setspecific (thread_name_key, "Regulator thread");

  while (!thread_pool_terminate_p (pool))
    {
      zfsd_mutex_lock (&pool->regulator_in_syscall);
      if (!thread_pool_terminate_p (pool))
	sleep (THREAD_POOL_REGULATOR_INTERVAL);//NOTE: condvar will be better
      zfsd_mutex_unlock (&pool->regulator_in_syscall);
      if (thread_pool_terminate_p (pool))
	break;
      zfsd_mutex_lock (&pool->mutex);
      thread_pool_regulate (pool);
      zfsd_mutex_unlock (&pool->mutex);
    }

  return NULL;
}
