/* Data coding functions (encoding and decoding requests and replies).
   Copyright (C) 2003 Josef Zlomek

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
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include "data-coding.h"
#include "log.h"
#include "memory.h"
#include "zfs_prot.h"

/* Initialize DC to start encoding to PTR with maximal length MAX_LENGTH.  */

void
start_encoding (DC *dc, void *ptr, int max_length)
{
#ifdef ENABLE_CHECKING
  if (ptr != ALIGN_PTR_16 (ptr))
    abort ();
#endif

  dc->original = (char *) ptr;
  dc->current = (char *) ptr;
  dc->max_length = max_length;
  dc->cur_length = 0;
  encode_uint32_t (dc, 0);
}

/* Update the size of block in DC.  */

int
finish_encoding (DC *dc)
{
  *(uint32_t *) dc->original = u32_to_le ((uint32_t) dc->cur_length);

  return dc->cur_length;
}

/* Initialize DC to start decoding of PTR.  */

void
start_decoding (DC *dc, void *ptr)
{
  dc->original = (char *) ptr;
  dc->current = (char *) ptr;
  dc->max_length = 4;
  dc->cur_length = 0;
  decode_int32_t (dc, (int32_t *) &dc->max_length);
}

/* Decode a value of type T and size S from DC and store it to *RET.
   Call F to transform little endian to cpu endian.
   Return true on success.  */
#define DECODE_SIMPLE_TYPE(T, S, F)				\
int								\
decode_##T (DC *dc, T *ret)					\
{								\
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    return 0;							\
								\
  dc->current = (char *) ALIGN_PTR_##S (dc->current);		\
  *ret = F (dc->current);					\
  dc->current += S;						\
								\
  return 1;							\
}

/* Encode a value VAL of type T and size S to DC.
   Call F to transform cpu endian to little endian.
   Return true on success.  */
#define ENCODE_SIMPLE_TYPE(T, S, F)				\
int								\
encode_##T (DC *dc, T val)					\
{								\
  int prev = dc->cur_length;					\
  char *s;							\
								\
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    {								\
      dc->cur_length = prev;					\
      return 0;							\
    }								\
								\
  /* Clear bytes which are before the aligned offset.  */	\
  s = dc->current;						\
  dc->current = (char *) ALIGN_PTR_##S (dc->current);		\
  while (s < dc->current)					\
    *s++ = 0;							\
								\
  *(T *) dc->current = F (val);					\
  dc->current += S;						\
								\
  return 1;							\
}

typedef unsigned char uchar;

DECODE_SIMPLE_TYPE (char, 1, *)
DECODE_SIMPLE_TYPE (uchar, 1, *)
DECODE_SIMPLE_TYPE (int16_t, 2, le_to_i16p)
DECODE_SIMPLE_TYPE (uint16_t, 2, le_to_u16p)
DECODE_SIMPLE_TYPE (int32_t, 4, le_to_i32p)
DECODE_SIMPLE_TYPE (uint32_t, 4, le_to_u32p)
DECODE_SIMPLE_TYPE (int64_t, 8, le_to_i64p)
DECODE_SIMPLE_TYPE (uint64_t, 8, le_to_u64p)

ENCODE_SIMPLE_TYPE (char, 1, )
ENCODE_SIMPLE_TYPE (uchar, 1, )
ENCODE_SIMPLE_TYPE (int16_t, 2, i16_to_le)
ENCODE_SIMPLE_TYPE (uint16_t, 2, u16_to_le)
ENCODE_SIMPLE_TYPE (int32_t, 4, i32_to_le)
ENCODE_SIMPLE_TYPE (uint32_t, 4, u32_to_le)
ENCODE_SIMPLE_TYPE (int64_t, 8, i64_to_le)
ENCODE_SIMPLE_TYPE (uint64_t, 8, u32_to_le)

int
decode_buffer (DC *dc, void **buf, uint32_t max_len)
{
  uint32_t len;

  if ((decode_uint32_t (dc, &len)) == 0)
    return 0;

  if (len > max_len)
    return 0;

  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    return 0;
  
  *buf = xmalloc (len);
  memcpy (*buf, dc->current, len);
  dc->current += len;

  return 1;
}

int
encode_buffer (DC *dc, void *buf, uint32_t len)
{
  int prev;

  if ((encode_uint32_t (dc, len)) == 0)
      return 0;

  prev = dc->cur_length;
  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return 0;
    }

  memcpy (dc->current, buf, len);
  dc->current += len;

  return 1;
}

int
decode_ftype (DC *dc, ftype *type)
{
  uchar type_val;
  int r;
  
  r = decode_uchar (dc, &type_val);
  *type = (ftype) type_val;

  return r;
}

int
encode_ftype (DC *dc, ftype type)
{
  return encode_uchar (dc, (uchar) type);
}

int
decode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (decode_uint32_t (dc, &fh->sid)
	  && decode_uint32_t (dc, &fh->vid)
	  && decode_uint32_t (dc, &fh->dev)
	  && decode_uint32_t (dc, &fh->ino));
}

int
encode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (encode_uint32_t (dc, fh->sid)
	  && encode_uint32_t (dc, fh->vid)
	  && encode_uint32_t (dc, fh->dev)
	  && encode_uint32_t (dc, fh->ino));
}

int
decode_zfs_time (DC *dc, zfs_time *time)
{
  return (decode_uint32_t (dc, &time->sec)
	  && decode_uint32_t (dc, &time->usec));

}


int
encode_zfs_time (DC *dc, zfs_time *time)
{
  return (encode_uint32_t (dc, time->sec)
	  && encode_uint32_t (dc, time->usec));
}

int
decode_fattr (DC *dc, fattr *attr)
{
  return (decode_ftype (dc, &attr->type)
	  && decode_uint32_t (dc, &attr->mode)
	  && decode_uint32_t (dc, &attr->nlink)
	  && decode_uint32_t (dc, &attr->uid)
	  && decode_uint32_t (dc, &attr->gid)
	  && decode_uint32_t (dc, &attr->rdev)
	  && decode_uint64_t (dc, &attr->size)
	  && decode_uint64_t (dc, &attr->blocks)
	  && decode_uint32_t (dc, &attr->blksize)
	  && decode_uint32_t (dc, &attr->generation)
	  && decode_uint64_t (dc, &attr->fversion)
	  && decode_uint32_t (dc, &attr->sid)
	  && decode_uint32_t (dc, &attr->vid)
	  && decode_uint32_t (dc, &attr->fsid)
	  && decode_uint32_t (dc, &attr->fileid)
	  && decode_zfs_time (dc, &attr->atime)
	  && decode_zfs_time (dc, &attr->mtime)
	  && decode_zfs_time (dc, &attr->ctime));
}

int
encode_fattr (DC *dc, fattr *attr)
{
  return (encode_ftype (dc, attr->type)
	  && encode_uint32_t (dc, attr->mode)
	  && encode_uint32_t (dc, attr->nlink)
	  && encode_uint32_t (dc, attr->uid)
	  && encode_uint32_t (dc, attr->gid)
	  && encode_uint32_t (dc, attr->rdev)
	  && encode_uint64_t (dc, attr->size)
	  && encode_uint64_t (dc, attr->blocks)
	  && encode_uint32_t (dc, attr->blksize)
	  && encode_uint32_t (dc, attr->generation)
	  && encode_uint64_t (dc, attr->fversion)
	  && encode_uint32_t (dc, attr->sid)
	  && encode_uint32_t (dc, attr->vid)
	  && encode_uint32_t (dc, attr->fsid)
	  && encode_uint32_t (dc, attr->fileid)
	  && encode_zfs_time (dc, &attr->atime)
	  && encode_zfs_time (dc, &attr->mtime)
	  && encode_zfs_time (dc, &attr->ctime));
}

int
decode_sattr (DC *dc, sattr *attr)
{
  return (decode_uint32_t (dc, &attr->mode)
	  && decode_uint32_t (dc, &attr->uid)
	  && decode_uint32_t (dc, &attr->gid)
	  && decode_uint64_t (dc, &attr->size)
	  && decode_zfs_time (dc, &attr->atime)
	  && decode_zfs_time (dc, &attr->mtime));
}

int
encode_sattr (DC *dc, sattr *attr)
{
  return (encode_uint32_t (dc, attr->mode)
	  && encode_uint32_t (dc, attr->uid)
	  && encode_uint32_t (dc, attr->gid)
	  && encode_uint64_t (dc, attr->size)
	  && encode_zfs_time (dc, &attr->atime)
	  && encode_zfs_time (dc, &attr->mtime));
}

int
decode_filename (DC *dc, char **s)
{
  return decode_buffer (dc, (void **) s, ZFS_MAXNAMELEN);
}

int
encode_filename (DC *dc, char *s, uint32_t len)
{
  return 0;
}

int
decode_sattr_args (DC *dc, sattr_args *args)
{
  return (decode_zfs_fh (dc, &args->file)
	  && decode_sattr (dc, &args->attributes));
}

int
encode_sattr_args (DC *dc, sattr_args *args)
{
  return (encode_zfs_fh (dc, &args->file)
	  && encode_sattr (dc, &args->attributes));
}

int
decode_dir_op_res (DC *dc, dir_op_res *res)
{
  return (decode_zfs_fh (dc, &res->file)
	  && decode_fattr (dc, &res->attributes));
}

int
encode_dir_op_res (DC *dc, dir_op_res *res)
{
  return (encode_zfs_fh (dc, &res->file)
	  && encode_fattr (dc, &res->attributes));
}
