/* Cyclic queue datatype.
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
#include <stdlib.h>
#include "pthread.h"
#include "queue.h"
#include "log.h"
#include "memory.h"

/* Initialize queue Q to be a queue with at most SIZE elements.  */

void
queue_create (queue *q, size_t size)
{
#ifdef ENABLE_CHECKING
  if (size == 0)
    abort ();
#endif

  zfsd_mutex_init (&q->mutex);
  zfsd_cond_init (&q->non_empty);
  q->queue = (size_t *) xmalloc (size * sizeof (size_t));
  q->size = size;
  q->nelem = 0;
  q->start = 0;
  q->end = 0;
}

/* Destroy the queue Q.  */

void
queue_destroy (queue *q)
{
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
#endif

  q->size = 0;
  zfsd_cond_destroy (&q->non_empty);
  zfsd_mutex_destroy (&q->mutex);
  free (q->queue);
}

/* Put an element ELEM to the queue Q.  */

void
queue_put (queue *q, size_t elem)
{
  CHECK_MUTEX_LOCKED (&q->mutex);
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
  if (q->nelem == q->size)
    abort ();
#endif

  q->queue[q->end] = elem;
  q->end++;
  if (q->end == q->size)
    q->end = 0;
  q->nelem++;
  zfsd_cond_signal (&q->non_empty);
}

/* Get an element from the queue Q.  */

size_t
queue_get (queue *q)
{
  size_t r;

  CHECK_MUTEX_LOCKED (&q->mutex);
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
#endif

  while (q->nelem == 0)
    zfsd_cond_wait (&q->non_empty, &q->mutex);

  r = q->queue[q->start];
  q->start++;
  if (q->start == q->size)
    q->start = 0;
  q->nelem--;

  return r;
}
