/*! Datatype for list of hardlinks.
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
#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"
#include "hardlink-list.h"
#include "memory.h"
#include "crc32.h"
#include "alloc-pool.h"

/*! Alloc pool of hardlink_list_entry.  */
static alloc_pool hardlink_list_pool;

/*! Mutex protecting hardlink_list_pool.  */
static pthread_mutex_t hardlink_list_mutex;

/*! Return hash value for hardlink X.  */

static hash_t
hardlink_list_hash (const void *x)
{
  return HARDLINK_LIST_HASH ((const hardlink_list_entry) x);
}

/*! Compare hardlinks X and Y.  */

static int
hardlink_list_eq (const void *x, const void *y)
{
  const hardlink_list_entry h1 = (const hardlink_list_entry) x;
  const hardlink_list_entry h2 = (const hardlink_list_entry) y;

  return (h1->parent_ino == h2->parent_ino
	  && h1->parent_dev == h2->parent_dev
	  && h1->name.len == h2->name.len
	  && strcmp (h1->name.str, h2->name.str) == 0);
}

/*! Create a new hardlink list with initial NELEM elements.  */

hardlink_list
hardlink_list_create (unsigned int nelem, pthread_mutex_t *mutex)
{
  hardlink_list hl;

  hl = (hardlink_list) xmalloc (sizeof (struct hardlink_list_def));
  hl->mutex = mutex;
  hl->htab = htab_create (nelem, hardlink_list_hash, hardlink_list_eq, NULL,
			  NULL);
  hl->first = NULL;
  hl->last = NULL;

  return hl;
}

/*! Empty the hardlink list HL.  */

void
hardlink_list_empty (hardlink_list hl)
{
  hardlink_list_entry entry, next;

  CHECK_MUTEX_LOCKED (hl->mutex);

  zfsd_mutex_lock (&hardlink_list_mutex);
  for (entry = hl->first; entry; entry = next)
    {
      next = entry->next;
      free (entry->name.str);
      pool_free (hardlink_list_pool, entry);
    }
  zfsd_mutex_unlock (&hardlink_list_mutex);

  hl->first = NULL;
  hl->last = NULL;
  htab_empty (hl->htab);
}

/*! Destroy hardlink list HL.  */

void
hardlink_list_destroy (hardlink_list hl)
{
  hardlink_list_entry entry, next;

  CHECK_MUTEX_LOCKED (hl->mutex);

  zfsd_mutex_lock (&hardlink_list_mutex);
  for (entry = hl->first; entry; entry = next)
    {
      next = entry->next;
      free (entry->name.str);
      pool_free (hardlink_list_pool, entry);
    }
  zfsd_mutex_unlock (&hardlink_list_mutex);

  htab_destroy (hl->htab);
  free (hl);
}

/*! Insert hardlink [PARENT_DEV, PARENT_INO, NAME] to hardlink list HL.
   If COPY is true make a copy of the NAME.
   Return true if it was really inserted.  */

bool
hardlink_list_insert (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, string *name, bool copy)
{
  hardlink_list_entry entry;
  void **slot;

  CHECK_MUTEX_LOCKED (hl->mutex);

  zfsd_mutex_lock (&hardlink_list_mutex);
  entry = (hardlink_list_entry) pool_alloc (hardlink_list_pool);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  entry->parent_dev = parent_dev;
  entry->parent_ino = parent_ino;
  entry->name = *name;

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
	  free (name->str);
	}
      return false;
    }

  if (copy)
    entry->name.str = (char *) xmemdup (name->str, entry->name.len + 1);

  *slot = entry;
  entry->next = NULL;
  entry->prev = hl->last;
  if (hl->last)
    hl->last->next = entry;
  hl->last = entry;
  if (hl->first == NULL)
    hl->first = entry;

  return true;
}

/*! Return true if hardlink [PARENT_DEV, PARENT_INO, NAME] is a member
   of hardlink list HL.  */

bool
hardlink_list_member (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, string *name)
{
  struct hardlink_list_entry_def entry;

  CHECK_MUTEX_LOCKED (hl->mutex);

  entry.parent_dev = parent_dev;
  entry.parent_ino = parent_ino;
  entry.name = *name;
  return (htab_find_with_hash (hl->htab, &entry, HARDLINK_LIST_HASH (&entry))
	  != NULL);
}

/*! Delete hardlink [PARENT_DEV, PARENT_INO, NAME] from hardlink list HL.
   Return true if it was really deleted.  */

bool
hardlink_list_delete (hardlink_list hl, uint32_t parent_dev,
		      uint32_t parent_ino, string *name)
{
  struct hardlink_list_entry_def entry;
  hardlink_list_entry del;
  void **slot;

  CHECK_MUTEX_LOCKED (hl->mutex);

  entry.parent_dev = parent_dev;
  entry.parent_ino = parent_ino;
  entry.name = *name;
  slot = htab_find_slot_with_hash (hl->htab, &entry,
				   HARDLINK_LIST_HASH (&entry), NO_INSERT);
  if (!slot)
    return false;

  del = (hardlink_list_entry) *slot;
  if (del->next)
    del->next->prev = del->prev;
  else
    hl->last = del->prev;
  if (del->prev)
    del->prev->next = del->next;
  else
    hl->first = del->next;

  free (del->name.str);
  zfsd_mutex_lock (&hardlink_list_mutex);
  pool_free (hardlink_list_pool, del);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  htab_clear_slot (hl->htab, slot);

  return true;
}

/*! Delete hardlink list entry ENTRY from hardlink list HL.
   Return true if it was really deleted.  */

bool
hardlink_list_delete_entry (hardlink_list hl, hardlink_list_entry entry)
{
  void **slot;

  CHECK_MUTEX_LOCKED (hl->mutex);

  slot = htab_find_slot_with_hash (hl->htab, entry,
				   HARDLINK_LIST_HASH (entry), NO_INSERT);
  if (!slot)
    return false;

  if (entry->next)
    entry->next->prev = entry->prev;
  else
    hl->last = entry->prev;
  if (entry->prev)
    entry->prev->next = entry->next;
  else
    hl->first = entry->next;

  free (entry->name.str);
  zfsd_mutex_lock (&hardlink_list_mutex);
  pool_free (hardlink_list_pool, entry);
  zfsd_mutex_unlock (&hardlink_list_mutex);
  htab_clear_slot (hl->htab, slot);

  return true;
}

/*! Return the length of hardlink list HL, i.e. the number of entries.  */

unsigned int
hardlink_list_length (hardlink_list hl)
{
  CHECK_MUTEX_LOCKED (hl->mutex);

  return hl->htab->n_elements - hl->htab->n_deleted;
}

/*! Print the hardlink list HL to file F.  */

void
print_hardlink_list (FILE *f, hardlink_list hl)
{
  hardlink_list_entry entry;

  for (entry = hl->first; entry; entry = entry->next)
    {
      fprintf (f, "  %" PRIu32 ",%" PRIu32 " %s\n",
	       entry->parent_dev, entry->parent_ino, entry->name.str);
    }
}

/*! Print the hardlink list HL to STDERR.  */

void
debug_hardlink_list (hardlink_list hl)
{
  print_hardlink_list (stderr, hl);
}

/*! Initialize data structures in HARDLINK-LIST.C.  */

void
initialize_hardlink_list_c (void)
{
  zfsd_mutex_init (&hardlink_list_mutex);
  hardlink_list_pool
    = create_alloc_pool ("hardlink_list_pool",
			 sizeof (struct hardlink_list_entry_def),
			 1020, &hardlink_list_mutex);
}

/*! Destroy data structures in HARDLINK-LIST.C.  */

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
