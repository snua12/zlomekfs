/*! \file
    \brief Datatype for list of hardlinks.  */

/* Copyright (C) 2004 Josef Zlomek

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

#ifndef HARDLINK_LIST_H
#define HARDLINK_LIST_H

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include "pthread.h"
#include "memory.h"
#include "hashtab.h"
#include "crc32.h"

/*! Hash function for hardlink list entry H.  */
#define HARDLINK_LIST_HASH(H)						    \
  (crc32_update (crc32_update (crc32_buffer ((H)->name.str, (H)->name.len), \
		 &(H)->parent_dev, sizeof (uint32_t)),			    \
   &(H)->parent_ino, sizeof (uint32_t)))

typedef struct hardlink_list_entry_def *hardlink_list_entry;

/*! \brief Entry of a hardlink list.  */
struct hardlink_list_entry_def
{
  /*! Next and previous entry in the doubly linked chain.  */
  hardlink_list_entry next;
  hardlink_list_entry prev;

  /*! Device of parent directory.  */
  uint32_t parent_dev;

  /*! Inode of parent directory.  */
  uint32_t parent_ino;

  /*! File name.  */
  string name;
};

/*! \brief A hashed variable-sized array.  */
typedef struct hardlink_list_def
{
  /*! Hash table.  */
  htab_t htab;

  /*! Mutex which must be locked when accessing the hardlink list.  */
  pthread_mutex_t *mutex;

  /*! First and last node of the doubly-linked chain.  */
  hardlink_list_entry first;
  hardlink_list_entry last;
} *hardlink_list;

extern hardlink_list hardlink_list_create (unsigned int nelem,
					   pthread_mutex_t *mutex);
extern void hardlink_list_empty (hardlink_list hl);
extern void hardlink_list_destroy (hardlink_list hl);
extern bool hardlink_list_insert (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, string *name, bool copy);
extern bool hardlink_list_member (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, string *name);
extern bool hardlink_list_delete (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, string *name);
extern bool hardlink_list_delete_entry (hardlink_list hl,
					hardlink_list_entry entry);
extern unsigned int hardlink_list_length (hardlink_list hl);
extern void print_hardlink_list (FILE *f, hardlink_list hl);
extern void debug_hardlink_list (hardlink_list hl);

extern void initialize_hardlink_list_c (void);
extern void cleanup_hardlink_list_c (void);

#endif
