/* Functions to support a pool of allocatable objects.
   Copyright (C) 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Daniel Berlin <dan@cgsoftware.com>

   Some modifications for ZFS:
   Copyright (C) 2003 Josef Zlomek (josef.zlomek@email.cz).

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

#ifndef ALLOC_POOL_H
#define ALLOC_POOL_H

#include "system.h"
#include <stddef.h>
#include "pthread.h"

typedef unsigned long ALLOC_POOL_ID_TYPE;

typedef struct alloc_pool_list_def
{
  struct alloc_pool_list_def *next;
} *alloc_pool_list;

typedef struct alloc_pool_def
{
  char *name;
  pthread_mutex_t *mutex;
#ifdef ENABLE_CHECKING
  ALLOC_POOL_ID_TYPE id;
#endif
  size_t elts_per_block;
  alloc_pool_list free_list;
  size_t elts_allocated;
  size_t elts_free;
  size_t blocks_allocated;
  alloc_pool_list block_list;
  size_t block_size;
  size_t elt_size;
} *alloc_pool;

extern alloc_pool create_alloc_pool (const char *name, size_t size, size_t num,
				     pthread_mutex_t *mutex);
extern void free_alloc_pool (alloc_pool pool);
extern void *pool_alloc (alloc_pool pool);
extern void pool_free (alloc_pool pool, void *ptr);

#endif
