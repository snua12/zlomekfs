/* Datatype for list of hardlinks.
   Copyright (C) 2004 Josef Zlomek

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
#include <inttypes.h>
#include <stdlib.h>
#include "hardlink-list.h"
#include "memory.h"
#include "crc32.h"
#include "alloc-pool.h"

/* Alloc pool of hardlink_list_entry.  */
static alloc_pool hardlink_list_pool;

/* Mutex protecting hardlink_list_pool.  */
static pthread_mutex_t hardlink_list_mutex;

/* Return hash value for hardlink X.  */

static hash_t
hardlink_list_hash (const void *x)
{
  return HARDLINK_LIST_HASH ((const hardlink_list_entry) x);
}

/* Compare hardlinks X and Y.  */

static int
hardlink_list_eq (const void *x, const void *y)
{
  const hardlink_list_entry h1 = (const hardlink_list_entry) x;
  const hardlink_list_entry h2 = (const hardlink_list_entry) y;

  return (h1->parent_ino == h2->parent_ino
	  && h1->parent_dev == h2->parent_dev
	  && strcmp (h1->name, h2->name) == 0);
}

/* Create a new hardlink list with initial NELEM elements.  */

hardlink_list
hardlink_list_create (unsigned int nelem, pthread_mutex_t *mutex)
{
  hardlink_list hl;

  hl = (hardlink_list) xmalloc (sizeof (struct hardlink_list_def));
  hl->mutex = mutex;

  /* Create varray.  */
  varray_create (&hl->array, sizeof (hardlink_list_entry), nelem);

  /* Create hashtab.  */
  hl->htab = htab_create (nelem, hardlink_list_hash, hardlink_list_eq, NULL,
			  NULL);

  return hl;
}

/* Destroy hardlink list HL.  */

void
hardlink_list_destroy (hardlink_list hl)
{
  CHECK_MUTEX_LOCKED (hl->mutex);

  zfsd_mutex_lock (&hardlink_list_mutex);
  while (VARRAY_USED (hl->array) > 0)
    {
      hardlink_list_entry del;

      del = VARRAY_TOP (hl->array, hardlink_list_entry);
      VARRAY_POP (hl->array);

      free (del->name);
      pool_free (hardlink_list_pool, del);
    }
  zfsd_mutex_unlock (&hardlink_list_mutex);

  varray_destroy (&hl->array);
  htab_destroy (hl->htab);
  free (hl);
}

/* Insert hardlink [PARENT_DEV, PARENT_INO, NAME] to hardlink list HL.
   If COPY is true make a copy of the NAME.
   Return true if it was really inserted.  */

bool
hardlink_list_insert (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, char *name, bool copy)
{
  hardlink_list_entry entry;
  void **slot;

  CHECK_MUTEX_LOCKED (hl->mutex);

  zfsd_mutex_lock (&hardlink_list_mutex);
  entry = (hardlink_list_entry) pool_alloc (hardlink_list_pool);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  entry->parent_dev = parent_dev;
  entry->parent_ino = parent_ino;
  entry->name = name;

  slot = htab_find_slot_with_hash (hl->htab, entry,
				   HARDLINK_LIST_HASH (entry), INSERT);
  if (*slot)
    {
      zfsd_mutex_lock (&hardlink_list_mutex);
      pool_free (hardlink_list_pool, entry);
      zfsd_mutex_unlock (&hardlink_list_mutex);

      if (!copy)
	{
	  /* If we shall not copy NAME the NAME is dynamically allocated
	     and caller does not free it so we have to free it now.  */
	  free (name);
	}
      return false;
    }

  entry->index = VARRAY_USED (hl->array);
  if (copy)
    entry->name = xstrdup (name);

  VARRAY_PUSH (hl->array, entry, hardlink_list_entry);
  *slot = entry;

  return true;
}

/* Return true if hardlink [PARENT_DEV, PARENT_INO, NAME] is a member
   of hardlink list HL.  */

bool
hardlink_list_member (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, char *name)
{
  struct hardlink_list_entry_def entry;

  CHECK_MUTEX_LOCKED (hl->mutex);

  entry.parent_dev = parent_dev;
  entry.parent_ino = parent_ino;
  entry.name = name;
  return (htab_find_with_hash (hl->htab, &entry, HARDLINK_LIST_HASH (&entry))
	  != NULL);
}

/* Delete hardlink [PARENT_DEV, PARENT_INO, NAME] from hardlink list HL.
   Return true if it was really deleted.  */

bool
hardlink_list_delete (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, char *name)
{
  struct hardlink_list_entry_def entry;
  hardlink_list_entry del, last;
  void **slot;

  CHECK_MUTEX_LOCKED (hl->mutex);

  entry.parent_dev = parent_dev;
  entry.parent_ino = parent_ino;
  entry.name = name;
  slot = htab_find_slot_with_hash (hl->htab, &entry,
				   HARDLINK_LIST_HASH (&entry), NO_INSERT);
  if (!slot)
    return false;

  del = (hardlink_list_entry) *slot;
  last = VARRAY_TOP (hl->array, hardlink_list_entry);
  if (del != last)
    {
      VARRAY_ACCESS (hl->array, del->index, hardlink_list_entry)
	= VARRAY_ACCESS (hl->array, last->index, hardlink_list_entry);
      last->index = del->index;
    }
  VARRAY_POP (hl->array);

  free (del->name);
  zfsd_mutex_lock (&hardlink_list_mutex);
  pool_free (hardlink_list_pool, del);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  htab_clear_slot (hl->htab, slot);

  return true;
}

/* Return the number of hardlinks in hardlink list HL.  */

unsigned int
hardlink_list_size (hardlink_list hl)
{
  CHECK_MUTEX_LOCKED (hl->mutex);

  return VARRAY_USED (hl->array);
}

/* Get element of the hardlink list HL at index INDEX.  */

hardlink_list_entry
hardlink_list_element (hardlink_list hl, unsigned int index)
{
  CHECK_MUTEX_LOCKED (hl->mutex);

  return VARRAY_ACCESS (hl->array, index, hardlink_list_entry);
}

/* Initialize data structures in HARDLINK-LIST.C.  */

void
initialize_hardlink_list_c (void)
{
  zfsd_mutex_init (&hardlink_list_mutex);
  hardlink_list_pool
    = create_alloc_pool ("hardlink_list_pool",
			 sizeof (struct hardlink_list_entry_def),
			 1020, &hardlink_list_mutex);
}

/* Destroy data structures in HARDLINK-LIST.C.  */

void
cleanup_hardlink_list_c (void)
{
  zfsd_mutex_lock (&hardlink_list_mutex);
#ifdef ENABLE_CHECKING
  if (hardlink_list_pool->elts_free < hardlink_list_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in hardlink_list_pool.\n",
	     hardlink_list_pool->elts_allocated - hardlink_list_pool->elts_free);
#endif
  free_alloc_pool (hardlink_list_pool);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  zfsd_mutex_destroy (&hardlink_list_mutex);
}
