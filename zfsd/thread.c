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
#include "queue.h"
#include "thread.h"

/* Initialize POOL to be a thread pool of MAX_THREADS threads with
   MIN_SPARE (MAX_THREADS) minimum (maximum) number of spare threads.  */

void
thread_pool_create (thread_pool *pool, size_t max_threads,
		    size_t min_spare_threads, size_t max_spare_threads)
{
  size_t i;

  pool->nthreads = 0;
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
