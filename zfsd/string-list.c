/* String list datatype.
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
#include "string-list.h"
#include "memory.h"
#include "crc32.h"
#include "log.h"
#include "alloc-pool.h"

/* Entry of a string list.  */
typedef struct string_list_entry_def
{
  /* Index of this struct in the varray.  */
  unsigned int index;

  /* String.  */
  char *str;
} *string_list_entry;

/* Alloc pool of string_list_entry.  */
static alloc_pool string_list_pool;

/* Mutex protecting string_list_pool.  */
static pthread_mutex_t string_list_mutex;

/* Return hash value for string X.  */

static hash_t
string_list_hash (const void *x)
{
  return STRING_LIST_HASH (((const string_list_entry) x)->str);
}

/* Compare hash table element X and string Y.  */

static int
string_list_eq (const void *x, const void *y)
{
  return (strcmp (((const string_list_entry) x)->str,
		  (const char *) y) == 0);
}

/* Create a new string list with initial NELEM elements.  */

string_list
string_list_create (unsigned int nelem, pthread_mutex_t *mutex)
{
  string_list sl;

  sl = (string_list) xmalloc (sizeof (struct string_list_def));
  sl->mutex = mutex;

  /* Create varray.  */
  varray_create (&sl->array, sizeof (string_list_entry), nelem);

  /* Create hashtab.  */
  sl->htab = htab_create (nelem, string_list_hash, string_list_eq, NULL,
			  mutex);

  return sl;
}

/* Destroy string list SL.  */

void
string_list_destroy (string_list sl)
{
  CHECK_MUTEX_LOCKED (sl->mutex);

  while (VARRAY_USED (sl->array))
    {
      string_list_entry del;

      del = VARRAY_TOP (sl->array, string_list_entry);
      VARRAY_POP (sl->array);

      free (del->str);
      zfsd_mutex_lock (&string_list_mutex);
      pool_free (string_list_pool, del);
      zfsd_mutex_unlock (&string_list_mutex);
    }

  varray_destroy (&sl->array);
  htab_destroy (sl->htab);
  free (sl);
}

/* Insert string STR to string list SL.  If COPY is true make a copy of the
   string.  Return true if STR was really inserted.  */

bool
string_list_insert (string_list sl, char *str, bool copy)
{
  string_list_entry entry;
  void **slot;

  CHECK_MUTEX_LOCKED (sl->mutex);

  slot = htab_find_slot_with_hash (sl->htab, str, STRING_LIST_HASH (str),
				   INSERT);
  if (*slot)
    {
      if (!copy)
	{
	  /* If we shall not copy string the string is dynamically allocated
	     and caller does not free it so we have to free it now.  */
	  free (str);
	}
      return false;
    }

  zfsd_mutex_lock (&string_list_mutex);
  entry = (string_list_entry) pool_alloc (string_list_pool);
  zfsd_mutex_unlock (&string_list_mutex);
  entry->index = VARRAY_USED (sl->array);
  if (copy)
    entry->str = xstrdup (str);
  else
    entry->str = str;

  VARRAY_PUSH (sl->array, entry, string_list_entry);
  *slot = entry;

  return true;
}

/* Return true if string STR is a member of string list SL.  */

bool
string_list_member (string_list sl, char *str)
{
  CHECK_MUTEX_LOCKED (sl->mutex);

  return (htab_find_with_hash (sl->htab, str, STRING_LIST_HASH (str)) != NULL);
}

/* Delete string STR from string list SL.  Return true if STR was really
   deleted.  */

bool
string_list_delete (string_list sl, char *str)
{
  string_list_entry del, last;
  void **slot;

  CHECK_MUTEX_LOCKED (sl->mutex);

  slot = htab_find_slot_with_hash (sl->htab, str, STRING_LIST_HASH (str),
				   NO_INSERT);
  if (!slot)
    return false;

  del = (string_list_entry) *slot;
  last = VARRAY_TOP (sl->array, string_list_entry);
  if (del != last)
    {
      VARRAY_ACCESS (sl->array, del->index, string_list_entry)
	= VARRAY_ACCESS (sl->array, last->index, string_list_entry);
      last->index = del->index;
    }
  VARRAY_POP (sl->array);

  free (del->str);
  zfsd_mutex_lock (&string_list_mutex);
  pool_free (string_list_pool, del);
  zfsd_mutex_unlock (&string_list_mutex);
  htab_clear_slot (sl->htab, slot);

  return true;
}

/* Return the number of strings in string list SL.  */

unsigned int
string_list_size (string_list sl)
{
  CHECK_MUTEX_LOCKED (sl->mutex);

  return VARRAY_USED (sl->array);
}

/* Get element of the string list SL at index INDEX.  */

char *
string_list_element (string_list sl, unsigned int index)
{
  CHECK_MUTEX_LOCKED (sl->mutex);

  return VARRAY_ACCESS (sl->array, index, string_list_entry);
}

/* Initialize data structures in STRING-LIST.C.  */

void
initialize_string_list_c (void)
{
  zfsd_mutex_init (&string_list_mutex);
  string_list_pool = create_alloc_pool ("string_list_pool",
					sizeof (struct string_list_entry_def),
					1020, &string_list_mutex);
}

/* Destroy data structures in STRING-LIST.C.  */

void
cleanup_string_list_c (void)
{
  zfsd_mutex_lock (&string_list_mutex);
#ifdef ENABLE_CHECKING
  if (string_list_pool->elts_free < string_list_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in string_list_pool.\n",
	     string_list_pool->elts_allocated - string_list_pool->elts_free);
#endif
  free_alloc_pool (string_list_pool);
  zfsd_mutex_unlock (&string_list_mutex);
  zfsd_mutex_destroy (&string_list_mutex);
}
