/* Dynamic queue datatype.
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

#ifndef QUEUE_H
#define QUEUE_H

#include "system.h"
#include <stddef.h>
#include "pthread.h"
#include "alloc-pool.h"

typedef struct queue_node_def
{
  struct queue_node_def *next;	/* next node in the chain */
  char data[1];			/* data */
} *queue_node;

typedef struct queue_def
{
  pthread_mutex_t mutex;	/* mutex for accessing the queue */
  pthread_cond_t non_empty;	/* cond. var. for waiting while (nelem == 0) */
  alloc_pool pool;		/* alloc pool for elements of the queue */
  unsigned int nelem;		/* number of elements in the queue */
  unsigned int size;		/* size of an element */
  queue_node first;		/* first node of the queue */
  queue_node last;		/* last node of the queue */
} queue;

extern void queue_create (queue *q, size_t size, size_t num);
extern void queue_destroy (queue *q);
extern void queue_put (queue *q, void *elem);
extern void queue_get (queue *q, void *elem);

#endif
