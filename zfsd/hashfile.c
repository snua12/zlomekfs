/* An expandable hash table in a file.
   Copyright (C) 2003, 2004 Josef Zlomek
   Based on hashfile.c

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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "pthread.h"
#include "hashfile.h"
#include "log.h"
#include "memory.h"
#include "util.h"
#include "data-coding.h"

/* Size of buffer used in hfile_expand.  */
#define HFILE_BUFFER_SIZE 0x4000

/* Read and return status of slot from HFILE on offset OFFSET.  */

static uint32_t
hfile_read_slot_status (hfile_t hfile, uint64_t offset)
{
  if ((uint64_t) lseek (hfile->fd, offset, SEEK_SET) != offset)
    return (uint32_t) -1;

  if (!full_read (hfile->fd, hfile->element, sizeof (uint32_t)))
    return (uint32_t) -1;

  return le_to_u32 (*(uint32_t *) hfile->element);
}

/* Read an element and return status of slot from HFILE on offset OFFSET.  */

static uint32_t
hfile_read_element (hfile_t hfile, uint64_t offset)
{
  if ((uint64_t) lseek (hfile->fd, offset, SEEK_SET) != offset)
    return (uint32_t) -1;

  if (!full_read (hfile->fd, hfile->element, hfile->element_size))
    return (uint32_t) -1;

  return le_to_u32 (*(uint32_t *) hfile->element);
}

/* Find an empty slot for hfile_expand. HASH is the hash value for the element
   to be inserted. Expects no deleted slots in the table.  */

static uint64_t
hfile_find_empty_slot (hfile_t hfile, hashval_t hash)
{
  unsigned int size;
  unsigned int index;
  uint64_t offset;
  uint32_t status;

#ifdef ENABLE_CHECKING
  if (hfile->fd < 0)
    abort ();
#endif

  size = hfile->size;
  index = hash % size;

  offset = (uint64_t) index * hfile->element_size + sizeof (hashfile_header);
  status = hfile_read_slot_status (hfile, offset);
  if (status == (uint32_t) -1)
    return 0;

  if (status == EMPTY_SLOT)
    return offset;
#ifdef ENABLE_CHECKING
  if (status == DELETED_SLOT)
    abort ();
  if (status != VALID_SLOT)
    abort ();
#endif

  for (;;)
    {
      index++;
      if (index >= size)
	index -= size;

      offset = ((uint64_t) index * hfile->element_size
		+ sizeof (hashfile_header));
      status = hfile_read_slot_status (hfile, offset);
      if (status == (uint32_t) -1)
	return 0;
      if (status == EMPTY_SLOT)
	return offset;
#ifdef ENABLE_CHECKING
      if (status == DELETED_SLOT)
	abort ();
      if (status != VALID_SLOT)
	abort ();
#endif
    }
}

/* Find a slot for ELEM. HASH is the hash value for the element
   to be inserted. Expects no deleted slots in the table.  */

static uint64_t
hfile_find_slot (hfile_t hfile, const void *elem, hashval_t hash, bool insert)
{
  unsigned int size;
  unsigned int index;
  uint64_t offset;
  uint64_t first_deleted_slot;
  uint32_t status;

#ifdef ENABLE_CHECKING
  if (hfile->fd < 0)
    abort ();
#endif

  size = hfile->size;
  index = hash % size;
  first_deleted_slot = 0;

  offset = (uint64_t) index * hfile->element_size + sizeof (hashfile_header);
  status = hfile_read_element (hfile, offset);
  if (status == (uint32_t) -1)
    return 0;
  if (status == EMPTY_SLOT)
    goto empty_slot;
  if (status == DELETED_SLOT)
    first_deleted_slot = offset;
  else
    {
#ifdef ENABLE_CHECKING
      if (status != VALID_SLOT)
	abort ();
#endif
      if ((*hfile->eq_f) (hfile->element, elem))
	return offset;
    }

  for (;;)
    {
      index++;
      if (index >= size)
	index -= size;

      offset = ((uint64_t) index * hfile->element_size
		+ sizeof (hashfile_header));
      status = hfile_read_element (hfile, offset);
      if (status == (uint32_t) -1)
	return 0;
      if (status == EMPTY_SLOT)
	goto empty_slot;
      if (status == DELETED_SLOT)
	{
	  if (!first_deleted_slot)
	    first_deleted_slot = offset;
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (status != VALID_SLOT)
	    abort ();
#endif
	  if ((*hfile->eq_f) (hfile->element, elem))
	    return offset;
	}
    }

empty_slot:
  if (!insert)
    return offset;

  if (first_deleted_slot)
    {
      hfile->n_deleted--;
      return first_deleted_slot;
    }

  hfile->n_elements++;
  return offset;
}

/* Expand or shrink hash file HFILE when necessary. Return true on success.  */

static bool
hfile_expand (hfile_t hfile)
{
  int old_fd;
  unsigned int new_size, old_size, pos;
  unsigned int chunk_size;
  unsigned int n_elements;
  char *new_path;
  hashfile_header header;
  char buffer[HFILE_BUFFER_SIZE];

  if (2 * (hfile->n_elements - hfile->n_deleted) >= hfile->size)
    new_size = 2 * hfile->size;
  else if (8 * (hfile->n_elements - hfile->n_deleted) <= hfile->size
	   && hfile->size > 32)
    new_size = hfile->size / 2;
  else if (2 * hfile->n_elements >= hfile->size)
    new_size = hfile->size;
  else
    return true;

  old_fd = hfile->fd;
  old_size = hfile->size;
  hfile->size = new_size;

  for (chunk_size = hfile->element_size;
       chunk_size <= HFILE_BUFFER_SIZE / 2;
       chunk_size *= 2)
    ;
  n_elements = chunk_size / hfile->element_size;
  if (n_elements > old_size)
    {
      n_elements = old_size;
      chunk_size = old_size * hfile->element_size;
    }

  new_path = xstrconcat (2, hfile->file_name, ".new");
  hfile->fd = open (new_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (hfile->fd < 0)
    goto hfile_expand_error;

  if (ftruncate (hfile->fd, ((uint64_t) hfile->size * hfile->element_size
			     + sizeof (hashfile_header))) < 0)
    goto hfile_expand_error_with_fd;

  header.n_elements = u32_to_le (hfile->n_elements - hfile->n_deleted);
  header.n_deleted = 0;
  if (!full_write (hfile->fd, &header, sizeof (header)))
    goto hfile_expand_error_with_fd;

  if ((uint64_t) lseek (old_fd, sizeof (hashfile_header), SEEK_SET)
      != sizeof (hashfile_header))
    goto hfile_expand_error_with_fd;

  /* Copy elements.  */
  for (pos = 0; pos < old_size; pos += n_elements)
    {
      char *element;
      unsigned int i;

      if (!full_read (old_fd, buffer, chunk_size))
	goto hfile_expand_error_with_fd;

      for (i = 0, element = buffer; i < n_elements;
	   i++, element += hfile->element_size)
	{
	  uint32_t status;
	  uint64_t offset;

	  status = le_to_u32 (*(uint32_t *) element);
	  if (status == VALID_SLOT)
	    {
	      offset = hfile_find_empty_slot (hfile,
					      (*hfile->hash_f) (element));
	      if (offset == 0)
		goto hfile_expand_error_with_fd;

	      if ((uint64_t) lseek (hfile->fd, offset, SEEK_SET) != offset)
		goto hfile_expand_error_with_fd;

	      if (!full_write (hfile->fd, element, hfile->element_size))
		goto hfile_expand_error_with_fd;
	    }
	}
    }

  if (rename (new_path, hfile->file_name) < 0)
    goto hfile_expand_error_with_fd;

  /* We have to preserve the file descriptor originally used.  */
#ifdef ENABLE_CHECKING
  if (dup2 (hfile->fd, old_fd) < 0)
    abort ();
#else
  dup2 (hfile->fd, old_fd);
#endif
  close (hfile->fd);
  hfile->fd = old_fd;

  hfile->n_elements -= hfile->n_deleted;
  hfile->n_deleted = 0;
  free (new_path);

  return true;

hfile_expand_error_with_fd:
  close (hfile->fd);
  unlink (new_path);

hfile_expand_error:
  hfile->fd = old_fd;
  hfile->size = old_size;
  free (new_path);

  return false;
}

/* Create the hash table data structure with SIZE elements, hash function
   HASH_F, compare function EQ_F, decode function DECODE_F, encode function
   ENCODE_F  and file FILE_NAME.  The size of the whole element is ELEMENT_SIZE,
   te size of base of element is BASE_SIZE*/

hfile_t
hfile_create (unsigned int element_size, unsigned int base_size,
	      unsigned int size,
	      hfile_hash hash_f, hfile_eq eq_f,
	      hfile_decode decode_f, hfile_encode encode_f,
	      const char *file_name, pthread_mutex_t *mutex)
{
  hfile_t hfile;

#ifdef ENABLE_CHECKING
  if (element_size < sizeof (hashfile_header))
    abort ();
  if (element_size > HFILE_BUFFER_SIZE)
    abort ();
  if (base_size > element_size)
    abort ();
  if (!hash_f)
    abort ();
  if (!eq_f)
    abort ();
#endif

  hfile = (hfile_t) xmalloc (sizeof (struct hfile_def));
  hfile->mutex = mutex;
  hfile->element = (char *) xmalloc (element_size);
  hfile->element_size = element_size;
  hfile->base_size = base_size;
  hfile->size = size;
  hfile->n_elements = 0;
  hfile->n_deleted = 0;
  hfile->hash_f = hash_f;
  hfile->eq_f = eq_f;
  hfile->decode_f = decode_f;
  hfile->encode_f = encode_f;
  hfile->file_name = xstrdup (file_name);
  hfile->fd = -1;
  hfile->generation = 0;

  return hfile;
}

/* Initialize hash file HFILE.  */

bool
hfile_init (hfile_t hfile, struct stat *st)
{
  hashfile_header header;

#ifdef ENABLE_CHECKING
  if (hfile->fd < 0)
    abort ();
#endif

  if (fstat (hfile->fd, st) < 0)
    return false;

  if ((st->st_mode & S_IFMT) != S_IFREG)
    return false;

  if ((uint64_t) st->st_size < (uint64_t) hfile->element_size)
    return false;

  if (!full_read (hfile->fd, &header, sizeof (header)))
    return false;

  hfile->n_elements = le_to_u32 (header.n_elements);
  hfile->n_deleted = le_to_u32 (header.n_deleted);
  hfile->size = (((uint64_t) st->st_size - hfile->element_size)
		 / hfile->element_size);

  return true;
}

/* Destroy the hash table HTAB.  */

void
hfile_destroy (hfile_t hfile)
{
  CHECK_MUTEX_LOCKED (hfile->mutex);

#ifdef ENABLE_CHECKING
  if (hfile->fd >= 0)
    abort ();
#endif

  free (hfile->file_name);
  free (hfile->element);
  free (hfile);
}

/* Lookup element X from hash file HFILE.  Return false on file failure.  */

bool
hfile_lookup (hfile_t hfile, void *x)
{
  uint64_t offset;
  uint32_t status;

  if (hfile->encode_f)
    (*hfile->encode_f) (x);

  offset = hfile_find_slot (hfile, x, (*hfile->hash_f) (x), false);
  if (!offset)
    {
      if (hfile->decode_f)
	(*hfile->decode_f) (x);
      return false;
    }

  status = le_to_u32 (*(uint32_t *) hfile->element);
  if (status == VALID_SLOT)
    {
      memcpy (x, hfile->element, hfile->element_size);
      if (hfile->decode_f)
	(*hfile->decode_f) (x);
    }
  else
    *(uint32_t *) x = status;

  return true;
}

/* Insert element X into hash file HFILE.  If BASE_ONLY is true insert only
   the base of the element.  Return false on file failure.  */

bool
hfile_insert (hfile_t hfile, void *x, bool base_only)
{
  uint64_t offset;
  uint32_t status;
  hashfile_header header;

  if (!hfile_expand (hfile))
    return false;

  if (hfile->encode_f)
    (*hfile->encode_f) (x);

  offset = hfile_find_slot (hfile, x, (*hfile->hash_f) (x), true);
  if (!offset)
    {
      if (hfile->decode_f)
	(*hfile->decode_f) (x);
      return false;
    }

  status = le_to_u32 (*(uint32_t *) hfile->element);
  *(uint32_t *) x = u32_to_le (VALID_SLOT);

  if ((uint64_t) lseek (hfile->fd, offset, SEEK_SET) != offset)
    goto hfile_insert_error;

  if (!full_write (hfile->fd, x, (base_only
				  ? hfile->base_size : hfile->element_size)))
    goto hfile_insert_error;

  if ((uint64_t) lseek (hfile->fd, 0, SEEK_SET) != 0)
    goto hfile_insert_error;

  header.n_elements = u32_to_le (hfile->n_elements);
  header.n_deleted = u32_to_le (hfile->n_deleted);
  if (!full_write (hfile->fd, &header, sizeof (header)))
    goto hfile_insert_error;

  if (hfile->decode_f)
    (*hfile->decode_f) (x);

  return true;

hfile_insert_error:
  if (status == DELETED_SLOT)
    hfile->n_deleted++;
  else
    hfile->n_elements--;

  if (hfile->decode_f)
    (*hfile->decode_f) (x);

  return false;
}

/* Delete element X from hash file HFILE.  Return false on file failure.  */

bool
hfile_delete (hfile_t hfile, void *x)
{
  uint64_t offset;
  uint32_t status;
  hashfile_header header;

  if (!hfile_expand (hfile))
    return false;

  if (hfile->encode_f)
    (*hfile->encode_f) (x);

  offset = hfile_find_slot (hfile, x, (*hfile->hash_f) (x), false);
  if (!offset)
    {
      if (hfile->decode_f)
	(*hfile->decode_f) (x);
      return false;
    }

  status = le_to_u32 (*(uint32_t *) hfile->element);
  if (status != VALID_SLOT)
    {
      if (hfile->decode_f)
	(*hfile->decode_f) (x);
      return true;
    }

  memset (hfile->element, 0, hfile->element_size);
  *(uint32_t *) hfile->element = u32_to_le (DELETED_SLOT);
  hfile->n_deleted++;

  if ((uint64_t) lseek (hfile->fd, offset, SEEK_SET) != offset)
    goto hfile_delete_error;

  if (!full_write (hfile->fd, hfile->element, hfile->element_size))
    goto hfile_delete_error;

  if ((uint64_t) lseek (hfile->fd, 0, SEEK_SET) != 0)
    goto hfile_delete_error;

  header.n_elements = u32_to_le (hfile->n_elements);
  header.n_deleted = u32_to_le (hfile->n_deleted);
  if (!full_write (hfile->fd, &header, sizeof (header)))
    goto hfile_delete_error;

  if (hfile->decode_f)
    (*hfile->decode_f) (x);

  return true;

hfile_delete_error:
  hfile->n_deleted--;

  if (hfile->decode_f)
    (*hfile->decode_f) (x);

  return false;
}
