/*! \file
    \brief An expandable hash table in a file.  */

/* Copyright (C) 2003, 2004 Josef Zlomek
   Based on hashtab.h

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANFILEILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef HASHFILE_H
#define HASHFILE_H

#include "system.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pthread.h"
#include "memory.h"

/*! Type of hash value.  */
typedef unsigned int hashval_t;

/*! Compute hash of an encoded element.  */
typedef hashval_t (*hfile_hash) (const void *x);

/*! Compare the encoded hash file element with possible element.  */
typedef int (*hfile_eq) (const void *x, const void *y);

/*! Decode element of the hash file.  */
typedef void (*hfile_decode) (void *x);

/*! Encode element of the hash file.  */
typedef void (*hfile_encode) (void *x);

/*! Hash table datatype.  */
typedef struct hfile_def
{
  /*! Mutex which must be locked when accessing the table.  */
  pthread_mutex_t *mutex;

  /*! Temporary buffer for one element.  */
  char *element;

  /*! Size of the whole element.  */
  unsigned int element_size;

  /*! Size if the base of the element.  */
  unsigned int base_size;

  /*! Size of the table (number of the entries).  */
  unsigned int size;

  /*! Current number of elements including deleted elements.  */
  unsigned int n_elements;

  /*! Current number of deleted elements.  */
  unsigned int n_deleted;

  /*! Hash function.  */
  hfile_hash hash_f;

  /*! Compare function.  */
  hfile_eq eq_f;

  /*! Decode function.  */
  hfile_decode decode_f;

  /*! Encode function.  */
  hfile_encode encode_f;

  /*! File name of the hash file.  */
  char *file_name;

  /*! File descriptor for the hash file.  */
  int fd;

  /*! Generation of file descriptor.  */
  unsigned int generation;
} *hfile_t;

/*! Header of the hash file.  */
typedef struct hashfile_header_def
{
  uint32_t n_elements;
  uint32_t n_deleted;
} hashfile_header;

/*! Status of the slot.  */
#define EMPTY_SLOT	0
#define DELETED_SLOT	1
#define VALID_SLOT	2

extern hfile_t hfile_create (unsigned int element_size, unsigned int base_size,
			     unsigned int size,
			     hfile_hash hash_f, hfile_eq eq_f,
			     hfile_decode decode_f, hfile_encode encode_f,
			     const char *file_name, pthread_mutex_t *mutex);
extern bool hfile_init (hfile_t hfile, struct stat *st);
extern void hfile_destroy (hfile_t hfile);
extern bool hfile_lookup (hfile_t hfile, void *x);
extern bool hfile_insert (hfile_t hfile, void *x, bool base_only);
extern bool hfile_delete (hfile_t hfile, void *x);

#endif
