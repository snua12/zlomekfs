/* An expandable hash tables datatype.
   Copyright (C) 1999, 2000, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Vladimir Makarov (vmakarov@cygnus.com).

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

#ifndef HASHTAB_H
#define HASHTAB_H

#include "system.h"
#include "pthread.h"

/* Insert operation.  */
enum insert
{
  NO_INSERT = 0,
  INSERT
};

/* Type of hash value.  */
typedef unsigned int hash_t;

/* Compute hash of a table entry.  */
typedef hash_t (*htab_hash) (const void *x);

/* Compare the hash table entry with possible entry.  */
typedef int (*htab_eq) (const void *x, const void *y);

/* Cleanup function called when element is deleted from hash table.  */
typedef void (*htab_del) (void *x);

/* Hash table datatype.  */
typedef struct htab_def
{
  /* Table itself.  */
  void **table;

  /* Size of the table (number of the entries).  */
  unsigned int size;

  /* Current number of elements including deleted elements.  */
  unsigned int n_elements;

  /* Current number of deleted elements.  */
  unsigned int n_deleted;

  /* Hash function.  */
  htab_hash hash_f;

  /* Compare function.  */
  htab_eq eq_f;

  /* Cleanup function.  */
  htab_del del_f;

  /* Mutex which must be locked when accessing the table.  */
  pthread_mutex_t *mutex;
} *htab_t;

extern htab_t htab_create (unsigned int size, htab_hash hash_f, htab_eq eq_f,
			   htab_del del_f, pthread_mutex_t *mutex);
extern void htab_destroy (htab_t htab);
extern void htab_empty (htab_t htab);
extern void htab_clear_slot (htab_t htab, void **slot);
extern void *htab_find (htab_t htab, const void *elem);
extern void *htab_find_with_hash (htab_t htab, const void *elem, hash_t hash);
extern void **htab_find_slot (htab_t htab, const void *elem,
			      enum insert insert);
extern void **htab_find_slot_with_hash (htab_t htab, const void *elem,
					hash_t hash, enum insert insert);

/* Value for empty hash table entry.  */
#define EMPTY_ENTRY ((void *) 0)

/* Value for deleted hash table entry.  */
#define DELETED_ENTRY ((void *) 1)

#ifdef ENABLE_CHECKING

/* Check the contents of SLOT is on correct position in HTAB.  */
#define HTAB_CHECK_SLOT(HTAB, SLOT)				\
  if (1)							\
    {								\
      hash_t hash_ = (*(HTAB)->hash_f) (*(SLOT));		\
      unsigned int size_ = (HTAB)->size;			\
      unsigned int index_ = hash_ % size_;			\
      unsigned int step_ = 1 + hash_ % (size_ - 2);		\
								\
      while (&(HTAB)->table[index_] != (SLOT))			\
	{							\
	  if ((HTAB)->table[index_] == EMPTY_ENTRY		\
	      || (HTAB)->table[index_] == *(SLOT))		\
	    abort ();						\
								\
	  index_ += step_;					\
	  if (index_ >= size_)					\
	    index_ -= size_;					\
	}							\
    }

/* Loop through all valid SLOTs of hash table HTAB.  */
#define HTAB_FOR_EACH_SLOT(HTAB, SLOT, CODE)			\
  CHECK_MUTEX_LOCKED ((HTAB)->mutex);				\
  for ((SLOT) = (HTAB)->table;					\
       (SLOT) < (HTAB)->table + (HTAB)->size;			\
       (SLOT)++)						\
    {								\
      if (*(SLOT) != EMPTY_ENTRY && *(SLOT) != DELETED_ENTRY)	\
	{							\
	  HTAB_CHECK_SLOT ((HTAB), (SLOT));			\
	  CODE;							\
	}							\
    }

/* Check the table HTAB.  */
#define HTAB_CHECK(HTAB)					\
  if (1)							\
    {								\
      void **slot_;						\
								\
      HTAB_FOR_EACH_SLOT ((HTAB), slot_, );			\
    }

#else

/* Check the contents of SLOT is on correct position in HTAB.  */
#define HTAB_CHECK_SLOT(HTAB, SLOT)

/* Loop through all valid SLOTs of hash table HTAB.  */
#define HTAB_FOR_EACH_SLOT(HTAB, SLOT, CODE)			\
  for ((SLOT) = (HTAB)->table;					\
       (SLOT) < (HTAB)->table + (HTAB)->size;			\
       (SLOT)++)						\
    {								\
      if (*(SLOT) != EMPTY_ENTRY && *(SLOT) != DELETED_ENTRY)	\
	{							\
	  CODE;							\
	}							\
    }

/* Check the table HTAB.  */
#define HTAB_CHECK(HTAB)

#endif

#endif
