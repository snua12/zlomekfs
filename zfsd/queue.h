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

#ifndef QUEUE_H
#define QUEUE_H

#include "system.h"
#include <stddef.h>
#include <pthread.h>

typedef struct queue_def
{
  pthread_rwlock_t lock;	/* lock for accessing the queue */
  size_t *queue;		/* the queue itself */
  size_t size;			/* size of the queue */
  size_t nelem;			/* number of elements in the queue */
  size_t start;			/* start of the queue */
  size_t end;			/* end of the queue */
} queue;

extern void queue_create (queue *q, size_t size);
extern void queue_destroy (queue *q);
extern inline void queue_put (queue *q, size_t elem);
extern inline size_t queue_get (queue *q);

#endif
