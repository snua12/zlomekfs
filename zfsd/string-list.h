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

#ifndef STRING_LIST_H
#define STRING_LIST_H

#include "system.h"
#include "pthread.h"
#include "hashtab.h"
#include "varray.h"
#include "crc32.h"

/* Hash function for string STR.  */
#define STRING_LIST_HASH(STR)	(crc32_string (STR))

/* Definition of the hashed variable-sized array.  */
typedef struct string_list_def
{
  /* Hash table.  */
  htab_t htab;

  /* Variable-length array.  */
  varray array;

  /* Mutex which must be locked when accessing the string list.  */
  pthread_mutex_t *mutex;
} *string_list;

extern string_list string_list_create (unsigned int nelem,
				       pthread_mutex_t *mutex);
extern void string_list_destroy (string_list sl);
extern void string_list_insert (string_list sl, char *str, bool copy);
extern bool string_list_member (string_list sl, char *str);
extern void string_list_delete (string_list sl, char *str);

#endif
