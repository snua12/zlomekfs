/* Expandable hash table datatype.
   Copyright (C) 2003 Josef Zlomek
   Inspired with gcc/libiberty/hashtab.c
   by Vladimir Makarov (vmakarov@cygnus.com)

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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#include <stdio.h>
#include "hashtab.h"

/* Value for empty hash table entry.  */
#define EMPTY_ENTRY ((void *) 0)

/* Value for deleted hash table entry.  */
#define DELETED_ENTRY ((void *) 1)

/* These are primes that are the highest primes lower than some power of 2.  */
static const unsigned int primes[] = {
  7,
  13,
  31,
  61,
  127,
  251,
  509,
  1021,
  2039,
  4093,
  8191,
  16381,
  32749,
  65521,
  131071,
  262139,
  524287,
  1048573,
  2097143,
  4194301,
  8388593,
  16777213,
};

#define N_PRIMES (sizeof(primes) / sizeof(primes[0]))
#define MAX_PRIME (primes[N_PRIMES - 1])

/* Return a prime number from the predecessing table which is greater
   or equal to N. */

static unsigned int
get_higher_prime(unsigned int n)
{
  unsigned int low = 0;
  unsigned int high = sizeof(primes) / sizeof(primes[0]) - 1;

  if (n > MAX_PRIME)
    {
      fprintf(stderr,
	      "%d is greater than maximum predefined prime number (%d).\n",
	      n, MAX_PRIME);
      return 0;
    }
  
  while (low != high)
    {
      unsigned int mid = (low + high) / 2;
      if (n > primes[mid])
	low = mid + 1;
      else
	high = mid;
    }
  return primes[low];
}

/* Find an empty slot for htab_expand. HASH is the hash value for the element
   to be inserted. Expects no deleted slots in the table.  */

static void **
htab_find_empty_slot(htab_t htab, hash_t hash)
{
  unsigned int size;
  unsigned int index;
  unsigned int step;
  void **slot;
  
  size = htab->size;
  index = hash % size;
  slot = htab->table + index;
  if (*slot == EMPTY_ENTRY)
    return slot;
#ifdef ENABLE_CHECKING
  if (*slot == DELETED_ENTRY)
    abort();
#endif

  step = 1 + hash % (size - 2);
  for (;;)
    {
      index += step;
      if (index >= size)
	index -= size;

      slot = htab->table + index;
      if (*slot == EMPTY_ENTRY)
	return slot;
#ifdef ENABLE_CHECKING
      if (*slot == DELETED_ENTRY)
	abort();
#endif
    }
}

static int
htab_expand(htab_t htab)
{
  void **new_table;
  void **old_table;
  unsigned int new_size, old_size, i;

  old_table = htab->table;
  old_size = htab->size;

  /* Get next prime number from table.  */
  new_size = get_higher_prime (htab->size + 1);
  new_table = (void **) calloc(new_size * sizeof(void *));
  if (!new_table)
    return 0;
  htab->table = new_table;
  htab->size = new_size;

  htab->n_elements -= htab->n_deleted;
  htab->n_deleted = 0;
 
  for (i = 0; i < old_size; i++)
    if (old_table[i] != EMPTY_ENTRY && old_table[i] != DELETED_ENTRY)
      {
	void **slot;

	slot = htab_find_empty_slot(htab, (*htab->hash_f)(old_table[i]));
	*slot = old_table[i];
      }
    }
  
  delete(old_table);
  return 1;
}

htab_t
htab_create(unsigned int size, htab_hash hash, htab_hash hash_f, htab_eq eq_f)
{
  htab_t htab;

  size = get_higher_prime(size);
  if (!size)
    return NULL;

  htab = (htab_t) malloc(sizeof(htab_t));
  if (!htab)
    return NULL;

  htab->table = (void **) calloc(size * sizeof(void *));
  if (!htab->table)
    {
      free(htab);
      return NULL;
    }
  
  htab->size = size;
  htab->n_elements = 0;
  htab->n_deleted = 0;
  htab->hash_f = hash_f;
  htab->eq_f = eq_f;
  return htab;
}

void
htab_destroy(htab_t htab, htab_del del_f)
{
  unsigned int i;
  if (del_f)
    {
      for (i = 0; i < htab->size; i++)
	if (htab->table[i] != EMPTY_ENTRY && htab->table[i] != DELETED_ENTRY)
	  (*del_f)(htab->table[i]);
    }
  free(htab->table);
  free(htab);
}

void
htab_clear_slot(htab_t htab, void **slot, htab_del del_f)
{
#ifdef ENABLE_CHECKING
  if (slot < htab->table || slot >= htab->table + htab->size
      || *slot == EMPTY_ENTRY || *slot == DELETED_ENTRY)
    abort();
#endif

  if (del_f)
    (*del_f)(*slot);

  *slot = DELETED_ENTRY;
  htab->n_deleted++;
}

void **
htab_find_slot(htab_t htab, const void *elem,
			     enum insert insert)
{
  return htab_find_slot_with_hash(htab, elem, (*htab->hash_f)(elem), insert);
}

void **
htab_find_slot_with_hash(htab_t htab, const void *elem,
				       hash_t hash, enum insert insert)
{
  unsigned int size;
  unsigned int index;
  unsigned int step;
  void **first_deleted_slot;

  if (insert == INSERT && htab->size * 2 <= htab->n_elements * 3
      && htab_expand(htab) == 0)
    return NULL;
  
  size = htab->size;
  index = hash % size;
  first_deleted_slot = NULL;

  if (htab->table[index] == EMPTY_ENTRY)
    goto empty_entry;
  if (htab->table[index] == DELETED_ENTRY)
    first_deleted_slot = &htab->table[index];
  else if ((*htab->eq_f)(htab->table[index], elem))
    return &htab->table[index];

  step = 1 + hash % (size - 2);
  for (;;)
    {
      index += step;
      if (index >= size)
	index -= size;

      if (htab->table[index] == EMPTY_ENTRY)
	goto empty_entry;
      if (hash->table[index] == DELETED_ENTRY)
	{
	  if (!first_deleted_slot)
	    first_deleted_slot = &htab->table[index];
	}
      else if ((*htab->eq_f)(htab->table[index], elem))
	return &htab->entries[index];
    }
  
empty_entry:
  if (insert == NO_INSERT)
    return NULL;

  htab->n_elements++;
  
  if (first_deleted_slot)
    {
      *first_deleted_slot = EMPTY_ENTRY;
      return first_deleted_slot;
    }

  return &htab->table[index];
}
