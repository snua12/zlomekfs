/*! Cyclic queue datatype.
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
#include <stdlib.h>
#include "pthread.h"
#include "queue.h"
#include "log.h"
#include "memory.h"

/*! Initialize queue Q to be a queue with elements of size SIZE.
   Alloc queue nodes in chunks of NUM nodes.  */

void
queue_create (queue *q, size_t size, size_t num, pthread_mutex_t *mutex)
{
#ifdef ENABLE_CHECKING
  if (size == 0)
    abort ();
  if (mutex == NULL)
    abort ();
#endif

  zfsd_cond_init (&q->non_empty);
  q->pool = create_alloc_pool ("queue_node", sizeof (void *) + size, num,
			       mutex);
  q->mutex = mutex;
  q->nelem = 0;
  q->size = size;
  q->first = NULL;
  q->last = NULL;
  q->exiting = false;
}

/*! Destroy the queue Q.  */

void
queue_destroy (queue *q)
{
  CHECK_MUTEX_LOCKED (q->mutex);
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
#endif

  q->size = 0;
  free_alloc_pool (q->pool);
  zfsd_cond_destroy (&q->non_empty);
}

/*! Put an element ELEM to the queue Q.  */

void
queue_put (queue *q, void *elem)
{
  queue_node node;

  CHECK_MUTEX_LOCKED (q->mutex);
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
#endif

  node = (queue_node) pool_alloc (q->pool);
  node->next = NULL;
  memcpy (node->data, elem, q->size);

  if (q->last)
    {
      q->last->next = node;
      q->last = node;
    }
  else
    {
      q->first = node;
      q->last = node;
    }

  q->nelem++;
  zfsd_cond_signal (&q->non_empty);
}

/*! Get an element from the queue Q and store it to ELEM.  */

bool
queue_get (queue *q, void *elem)
{
  queue_node node;

  CHECK_MUTEX_LOCKED (q->mutex);
#ifdef ENABLE_CHECKING
  if (q->size == 0)
    abort ();
#endif

  if (q->nelem == 0)
    {
      while (q->nelem == 0 && !q->exiting)
	zfsd_cond_wait (&q->non_empty, q->mutex);

      if (q->exiting)
	return false;
    }

  node = q->first;
#ifdef ENABLE_CHECKING
  if (!node)
    abort ();
#endif

  if (q->first == q->last)
    q->last = NULL;

  q->first = q->first->next;
  q->nelem--;
  memcpy (elem, node->data, q->size);
  return true;
}

/*! Tell the queue we are exiting, i.e. wake up threads waiting for an element
   to be added to the queue.  */

void
queue_exiting (queue *q)
{
  zfsd_mutex_lock (q->mutex);
  q->exiting = true;
  zfsd_cond_broadcast (&q->non_empty);
  zfsd_mutex_unlock (q->mutex);
}
