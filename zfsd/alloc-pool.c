/*! Functions to support a pool of allocatable objects.
   Copyright (C) 1987, 1997, 1998, 1999, 2000, 2001, 2003
   Free Software Foundation, Inc.
   Contributed by Daniel Berlin <dan@cgsoftware.com>

   Some modifications for ZFS:
   Copyright (C) 2003, 2004 Josef Zlomek (josef.zlomek@email.cz).

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
#include <inttypes.h>
#include "pthread.h"
#include "alloc-pool.h"
#include "log.h"
#include "memory.h"

#define align_four(x) (((x+3) >> 2) << 2)
#define align_eight(x) (((x+7) >> 3) << 3)

/*! The internal allocation object.  */
typedef struct allocation_object_def
{
#ifdef ENABLE_CHECKING
  /* The ID of alloc pool which the object was allocated from.  */
  ALLOC_POOL_ID_TYPE id;
#endif

  union
    {
      /* The data of the object.  */
      char data[1];

      /* Because we want any type of data to be well aligned after the ID,
	 the following elements are here.  They are never accessed so
	 the allocated object may be even smaller than this structure.  */
      char *align_p;
      double align_d;
      uint64_t align_i;
#ifdef HAVE_LONG_DOUBLE
      long double align_ld;
#endif
    } u;
} allocation_object;

/*! Offset of user data in the allocation object.  */
#define DATA_OFFSET (offsetof (allocation_object, u.data))

/*! Convert a pointer to allocation_object from a pointer to user data.  */
#define ALLOCATION_OBJECT_PTR_FROM_USER_PTR(X)				\
   ((allocation_object *) (((char *) (X)) - DATA_OFFSET))

/*! Convert a pointer to user data from a pointer to allocation_object.  */
#define USER_PTR_FROM_ALLOCATION_OBJECT_PTR(X)				\
   ((void *) (((allocation_object *) (X))->u.data))

#ifdef ENABLE_CHECKING
/*! Last used ID.  */
static ALLOC_POOL_ID_TYPE last_id;
#endif

/*! Create a pool of things of size SIZE, with NUM in each block we
   allocate.  */

alloc_pool
create_alloc_pool (const char *name, size_t size, size_t num,
		   pthread_mutex_t *mutex)
{
  alloc_pool pool;
  size_t header_size;

#ifdef ENABLE_CHECKING
  if (!name)
    abort ();
  if (num == 0)
    abort ();
#endif

  /* Make size large enough to store the list header.  */
  if (size < sizeof (alloc_pool_list))
    size = sizeof (alloc_pool_list);

  /* Now align the size to a multiple of 4.  */
  size = align_four (size) + DATA_OFFSET;

  /* Now init the various pieces of our pool structure.  */
  pool = (alloc_pool) xmalloc (sizeof (struct alloc_pool_def));
  pool->name = xstrdup (name);
  pool->mutex = mutex;
  pool->elt_size = size;
  pool->elts_per_block = num;

  /* List header size should be a multiple of 8 */
  header_size = align_eight (sizeof (struct alloc_pool_list_def));

  pool->block_size = (size * num) + header_size;
  pool->free_list = NULL;
  pool->elts_allocated = 0;
  pool->elts_free = 0;
  pool->blocks_allocated = 0;
  pool->block_list = NULL;

#ifdef ENABLE_CHECKING
  /* Increase the last used ID and use it for this pool.
     ID == 0 is used for free elements of pool so skip it.  */
  last_id++;
  if (last_id == 0)
    last_id++;

  pool->id = last_id;
#endif

  return (pool);
}

/*! Free all memory allocated for the given memory pool.  */
void
free_alloc_pool (alloc_pool pool)
{
  alloc_pool_list block, next_block;

#ifdef ENABLE_CHECKING
  if (!pool)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (pool->mutex);

  /* Free each block allocated to the pool.  */
  for (block = pool->block_list; block != NULL; block = next_block)
    {
      next_block = block->next;
      free (block);
    }
  /* Lastly, free the pool and the name.  */
  free (pool->name);
  free (pool);
}

/*! Allocates one element from the pool specified.  */
void *
pool_alloc (alloc_pool pool)
{
  alloc_pool_list header;
  char *block;

#ifdef ENABLE_CHECKING
  if (!pool)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (pool->mutex);

  /* If there are no more free elements, make some more!.  */
  if (!pool->free_list)
    {
      size_t i;
      alloc_pool_list block_header;

      /* Make the block */
      block = (char *) xmalloc (pool->block_size);
      block_header = (alloc_pool_list) block;
      block += align_eight (sizeof (struct alloc_pool_list_def));

      /* Throw it on the block list */
      block_header->next = pool->block_list;
      pool->block_list = block_header;

      /* Now put the actual block pieces onto the free list.  */
      for (i = 0; i < pool->elts_per_block; i++, block += pool->elt_size)
	{
#ifdef ENABLE_CHECKING
	  /* Mark the element to be free.  */
	  ((allocation_object *) block)->id = 0;
#endif
	  header = (alloc_pool_list) USER_PTR_FROM_ALLOCATION_OBJECT_PTR (block);
	  header->next = pool->free_list;
	  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (block, pool->elt_size));
	  pool->free_list = header;
	}
      /* Also update the number of elements we have free/allocated, and
	 increment the allocated block count.  */
      pool->elts_allocated += pool->elts_per_block;
      pool->elts_free += pool->elts_per_block;
      pool->blocks_allocated += 1;
    }

  /* Pull the first free element from the free list, and return it.  */
  header = pool->free_list;
  VALGRIND_DISCARD (VALGRIND_MAKE_READABLE (header, sizeof (void *)));
  pool->free_list = header->next;
  VALGRIND_MAKE_WRITABLE (header, pool->elt_size - DATA_OFFSET);
  pool->elts_free--;

#ifdef ENABLE_CHECKING
  /* Set the ID for element.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_WRITABLE (ALLOCATION_OBJECT_PTR_FROM_USER_PTR
					    (header), sizeof (pool->id)));
  ALLOCATION_OBJECT_PTR_FROM_USER_PTR (header)->id = pool->id;
  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (ALLOCATION_OBJECT_PTR_FROM_USER_PTR
					    (header), sizeof (pool->id)));
#endif

  message (4, stderr, "POOL ALLOC %s %p %p\n", pool->name, (void *) pool,
	   (void *) header);

  return ((void *) header);
}

/*! Puts PTR back on POOL's free list.  */
void
pool_free (alloc_pool pool, void *ptr)
{
  alloc_pool_list header;

#ifdef ENABLE_CHECKING
  if (!pool)
    abort ();
#endif

  message (4, stderr, "POOL FREE %s %p %p\n", pool->name, (void *) pool, ptr);

#ifdef ENABLE_CHECKING
  if (!ptr)
    abort ();

  CHECK_MUTEX_LOCKED (pool->mutex);

  /* Check whether the PTR was allocated from POOL.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_READABLE (ALLOCATION_OBJECT_PTR_FROM_USER_PTR
					    (ptr), sizeof (pool->id)));
  if (pool->id != ALLOCATION_OBJECT_PTR_FROM_USER_PTR (ptr)->id)
    abort ();
  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (ALLOCATION_OBJECT_PTR_FROM_USER_PTR
					    (ptr), sizeof (pool->id)));

#ifndef ENABLE_VALGRIND_CHECKING
  /* Mark the element to be free and the value to be invalid.  */
  memset (ALLOCATION_OBJECT_PTR_FROM_USER_PTR (ptr), 0, pool->elt_size);
#endif
#endif

  header = (alloc_pool_list) ptr;
  header->next = pool->free_list;
  VALGRIND_MAKE_NOACCESS (header, pool->elt_size - DATA_OFFSET);
  pool->free_list = header;
  pool->elts_free++;
}
