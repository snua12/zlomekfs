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
#include "memory.h"
#include "log.h"
#include "queue.h"
#include "thread.h"

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

  for (i = 0; i < max_threads; i++)
    {
      pool->threads[i].t.state = THREAD_DEAD;
      pool->threads[i].t.index = i;
      queue_put (&pool->empty, i);
    }
}

/* Kill/create threads when there are too many or not enough idle threads.
   It expects SERVER_POOL.IDLE.MUTEX to be locked.  */

void
thread_pool_regulate (thread_pool *pool, thread_start start)
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
	  size_t index = queue_get (&pool->idle);
	  pool->threads[index].t.state = THREAD_DYING;
	  pthread_mutex_unlock (&pool->threads[index].t.mutex);
	  if (pthread_join (pool->threads[index].t.thread_id, NULL) == 0)
	    {
	      /* Thread left the mutex locked.  */
	      pthread_mutex_unlock (&pool->threads[index].t.mutex);
	      pthread_mutex_destroy (&pool->threads[index].t.mutex);

	      pool->threads[index].t.state = THREAD_DEAD;
	      queue_put (&pool->empty, index);
	    }
	  else
	    {
	      message (-1, stderr, "pthread_join() failed\n");
	    }
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
	  (*start) ();
	}
      pthread_mutex_unlock (&pool->empty.mutex);
    }
}
