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

#ifndef HARDLINK_LIST_H
#define HARDLINK_LIST_H

#include "system.h"
#include <inttypes.h>
#include "pthread.h"
#include "hashtab.h"
#include "varray.h"
#include "crc32.h"

/* Hash function for hardlink list entry H.  */
#define HARDLINK_LIST_HASH(H)						\
  (crc32_update (crc32_update (crc32_string ((H)->name),		\
		 &(H)->parent_dev, sizeof (uint32_t)),			\
   &(H)->parent_ino, sizeof (uint32_t)))

/* Definition of the hashed variable-sized array.  */
typedef struct hardlink_list_def
{
  /* Hash table.  */
  htab_t htab;

  /* Variable-length array.  */
  varray array;

  /* Mutex which must be locked when accessing the hardlink list.  */
  pthread_mutex_t *mutex;
} *hardlink_list;

/* Entry of a hardlink list.  */
typedef struct hardlink_list_entry_def
{
  /* Index of this struct in the varray.  */
  unsigned int index;

  /* Device of parent directory.  */
  uint32_t parent_dev;

  /* Inode of parent directory.  */
  uint32_t parent_ino;

  /* File name.  */
  char *name;
} *hardlink_list_entry;

extern hardlink_list hardlink_list_create (unsigned int nelem,
					   pthread_mutex_t *mutex);
extern void hardlink_list_destroy (hardlink_list hl);
extern bool hardlink_list_insert (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, char *name, bool copy);
extern bool hardlink_list_member (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, char *name);
extern bool hardlink_list_delete (hardlink_list hl, uint32_t parent_dev,
				  uint32_t parent_ino, char *name);
extern unsigned int hardlink_list_size (hardlink_list hl);
extern hardlink_list_entry hardlink_list_element (hardlink_list hl,
						  unsigned int index);

extern void initialize_hardlink_list_c (void);
extern void cleanup_hardlink_list_c (void);

#endif
