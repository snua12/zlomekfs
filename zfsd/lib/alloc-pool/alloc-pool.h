/*! \file \brief Functions to support a pool of allocatable objects.  */

/* Copyright (C) 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Daniel Berlin <dan@cgsoftware.com>

   Some modifications for ZFS: Copyright (C) 2003 Josef Zlomek
   (josef.zlomek@email.cz).

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#ifndef ALLOC_POOL_H
#define ALLOC_POOL_H

#include "system.h"
#include <stddef.h>
#include "pthread-wrapper.h"
#include "log.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ENABLE_CHECKING
	/*! Type of ID of the alloc pool.  */
	typedef uint32_t alloc_pool_id_t;
#endif

	/*! \brief Structure chaining free elements.  */
	typedef struct alloc_pool_list_def
	{
		struct alloc_pool_list_def *next;	/*!< Pointer to next element. */
	} *alloc_pool_list;

	/*! \brief Definition of alloc pool.  */
	typedef struct alloc_pool_def
	{
		char *name;				/*!< Name of the pool. */
		pthread_mutex_t *mutex;	/*!< Mutex. */
#ifdef ENABLE_CHECKING
		alloc_pool_id_t id;		/*!< ID. */
#endif
		size_t elts_per_block;	/*!< Elements per block. */
		alloc_pool_list free_list;	/*!< List of free elements. */
		size_t elts_allocated;	/*!< Number of allocated elements. */
		size_t elts_free;		/*!< Number of free elements. */
		size_t blocks_allocated;	/*!< Number of allocated blocks. */
		alloc_pool_list block_list;	/*!< List of blocks. */
		size_t block_size;		/*!< Size of block. */
		size_t elt_size;		/*!< Size of element. */
	} *alloc_pool;

	extern alloc_pool create_alloc_pool(const char *name, size_t size,
										size_t num, pthread_mutex_t * mutex);
	extern void free_alloc_pool(alloc_pool pool);
	extern void *pool_alloc(alloc_pool pool);
	extern void pool_free(alloc_pool pool, void *ptr);

#ifdef __cplusplus
}
#endif

#endif
