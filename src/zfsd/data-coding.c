/*! \file
    \brief Data coding functions (encoding and decoding requests and replies).

    Each request or reply is represented as "packet", a sequence of primitive
    values.

    All integer values use the little-endian two's complement representation,
    and their offset within the "packet" is aligned to the size of the integer
    (a 32-bit integer is aligned to 4 bytes, for example); the padding, if any,
    is filled with zeroes.

    "Data buffers" (read or write command data) are represented as data length
    (encoded as \c uint32_t) immediately followed by data of the specified
    length.

    Strings are represented as string length (encoded as \c uint32_t) followed
    by the string data and by a zero byte.  The zero byte is not counted in the
    string length.

    Other commonly used data types:
    - #zfs_fh:
      - <tt>uint32_t sid, vid</tt>
      - <tt>uint32_t dev, ino</tt>
      - <tt>uint32_t gen</tt>
    - #zfs_cap:
      - <tt>zfs_fh fh</tt>
      - <tt>uint32_t flags</tt>: #O_RDONLY or #O_WRONLY or #O_RDWR
      - <tt>uint8_t[#ZFS_VERIFY_LEN] verify</tt>
    - #fattr:
      - <tt>uint32_t dev, ino</tt>
      - <tt>uint64_t version</tt>
      - <tt>uint8_t ftype</tt>
      - <tt>uint32_t mode</tt>
      - <tt>uint32_t nlink</tt>
      - <tt>uint32_t uid, gid</tt>
      - <tt>uint32_t rdev</tt>
      - <tt>uint64_t size</tt>
      - <tt>uint64_t blocks</tt>
      - <tt>uint32_t blksize</tt>
      - <tt>zfs_time atime, mtime, ctime</tt>
    - #sattr:
      - <tt>uint32_t mode</tt>
      - <tt>uint32_t uid, gid</tt>
      - <tt>uint64_t size</tt>
      - <tt>zfs_time atime, mtime</tt>

    Each "packet" starts with the following header:

    - <tt>uint32_t length</tt>: the total packet length, including the header.
      The maximum allowed packet length is #DC_SIZE.
    - <tt>uint8_t direction</tt>: #direction_def
    - <tt>uint32_t request_id</tt>: ID of this request, or of the request this
      is a reply to if \p direction is #DIR_REPLY

    In #DIR_REQUEST and #DIR_ONEWAY packets the header is followed by:
    - <tt>uint32_t function</tt>: request function number
    - function-specific parameters

    In #DIR_REPLY packets the header is followed by:
    - <tt>int32_t status</tt>
    - function-specific return values.  These are omitted if \c status is not
      #ZFS_OK.

    Descriptions of the specific functions are contained in zfsd/zfs-prot.def.

    Possible protocol changes:
    - time, inode numbers should be 64-bit; what about device numbers?
    - O_* in capability flags should not depend on platform ABI
    */

/* Copyright (C) 2003, 2004 Josef Zlomek
   Copyright (C) 2004 Martin Zlomek

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

#ifdef __KERNEL__
# include <linux/types.h>
# include <linux/string.h>
# include <linux/slab.h>
# include <linux/vmalloc.h>
# include <asm/semaphore.h>
# include <asm/uaccess.h>
# include "zfs.h"
#else
# include <unistd.h>
# include <inttypes.h>
# include <string.h>
# include <stdio.h>
# include <stdlib.h>
# include "log.h"
# include "util.h"
# include "md5.h"
#endif

#include "data-coding.h"
#include "memory.h"
#include "zfs-prot.h"

#ifndef __KERNEL__

/*! Initialize a data coding buffer DC.  */

void
dc_init (DC *dc)
{
  dc->buffer = (char *) ALIGN_PTR_16 (dc->data);
}

#endif

#if defined(__KERNEL__) && defined(DEBUG)
static int allocated;
#endif

/*! Return a new data coding buffer.  */

DC *
dc_create (void)
{
  DC *dc;

#ifdef __KERNEL__
  dc = (DC *) vmalloc (sizeof (DC));
  if (dc) {
    TRACE("%d DCs allocated", ++allocated);
    dc->buffer = (char *) ALIGN_PTR_16 (dc->data);
  }
#else
  dc = (DC *) xmalloc (sizeof (DC));
  dc->buffer = (char *) ALIGN_PTR_16 (dc->data);
#endif

  return dc;
}

/*! Free the data coding buffer DC.  */

void
dc_destroy (DC *dc)
{
#ifdef __KERNEL__
  TRACE("%d DCs allocated", --allocated);
  vfree (dc);
#else
  free (dc);
#endif
}

#ifdef __KERNEL__

DECLARE_MUTEX (dc_lock);
static DC *dc[MAX_FREE_DCS];
static int ndc;

/*! Return a new data coding buffer;
   use an unused one or create a new one.  */

DC *
dc_get (void)
{
  DC *_dc;

  down (&dc_lock);
  if (ndc)
    {
      _dc = dc[--ndc];
      up (&dc_lock);
      return _dc;
    }
  else
    {
      up (&dc_lock);
      return dc_create();
    }
}

/*! Put back the data coding buffer DC;
   make it unused or free it.  */

void
dc_put (DC *_dc)
{
  if (_dc)
    {
      down (&dc_lock);
      if (!channel.connected || (ndc == MAX_FREE_DCS))
        {
          up (&dc_lock);
          dc_destroy(_dc);
        }
      else
        {
          dc[ndc++] = _dc;
          up (&dc_lock);
        }
    }
}

/*! Free all unused data coding buffers. */

void
dc_destroy_all (void)
{
  down (&dc_lock);
  while (ndc)
    dc_destroy (dc[--ndc]);
  up (&dc_lock);
}

#else

/*! Print DC to file F.  */

void
print_dc (DC *dc, FILE *f)
{
  fprintf (f, "Cur.pos    = %d\n", dc->cur_pos - dc->buffer);
  fprintf (f, "Cur.length = %d\n", dc->cur_length);
  fprintf (f, "Max.length = %d\n", dc->max_length);
  fprintf (f, "Data:\n");
  print_hex_buffer (dc->buffer,
                    (dc->max_length == DC_SIZE
                     ? dc->cur_length : dc->max_length),
                    f);
}

/*! Print DC to STDERR.  */

void
debug_dc (DC *dc)
{
  print_dc (dc, stderr);
}

#endif

/*! Initialize DC to start encoding to PTR with maximal length MAX_LENGTH.  */

void
start_encoding (DC *dc)
{
  dc->cur_pos = dc->buffer;
  dc->cur_length = 0;
  dc->max_length = DC_SIZE;
  encode_uint32_t (dc, 0);
}

/*! Update the size of block in DC.  Return the length of encoded buffer.  */

unsigned int
finish_encoding (DC *dc)
{
  *(uint32_t *) dc->buffer = u32_to_le ((uint32_t) dc->cur_length);

  return dc->cur_length;
}

/*! Initialize DC to start decoding of PTR.  Return true on success.  */

bool
start_decoding (DC *dc)
{
  dc->cur_pos = dc->buffer;
  dc->max_length = 4;
  dc->cur_length = 0;
  decode_uint32_t (dc, (uint32_t *) &dc->max_length);
  return dc->max_length <= DC_SIZE;
}

/*! Return true if all data has been read from encoded buffer.  */

bool
finish_decoding (DC *dc)
{
  return dc->cur_length == dc->max_length;
}

/*! Decode a value of type T and size S from DC and store it to *RET.
   Call F to transform little endian to cpu endian.
   Return true on success.  */
#define DECODE_SIMPLE_TYPE(T, S, F)				\
bool								\
decode_##T (DC *dc, T *ret)					\
{								\
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    return false;						\
                                                                \
  dc->cur_pos = (char *) ALIGN_PTR_##S (dc->cur_pos);		\
  *ret = F (dc->cur_pos);					\
  dc->cur_pos += S;						\
                                                                \
  return true;							\
}

/*! Encode a value VAL of type T and size S to DC.
   Call F to transform cpu endian to little endian.
   Return true on success.  */
#define ENCODE_SIMPLE_TYPE(T, S, F)				\
bool								\
encode_##T (DC *dc, T val)					\
{								\
  unsigned int prev = dc->cur_length;				\
  char *s;							\
                                                                \
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    {								\
      dc->cur_length = prev;					\
      return false;						\
    }								\
                                                                \
  /* Clear bytes which are before the aligned offset.  */	\
  s = dc->cur_pos;						\
  dc->cur_pos = (char *) ALIGN_PTR_##S (dc->cur_pos);		\
  while (s < dc->cur_pos)					\
    *s++ = 0;							\
                                                                \
  *(T *) dc->cur_pos = F (val);					\
  dc->cur_pos += S;						\
                                                                \
  return true;							\
}

DECODE_SIMPLE_TYPE (char, 1, *)
DECODE_SIMPLE_TYPE (uchar, 1, *)
#ifndef __KERNEL__
DECODE_SIMPLE_TYPE (int16_t, 2, le_to_i16p)
DECODE_SIMPLE_TYPE (uint16_t, 2, le_to_u16p)
#endif
DECODE_SIMPLE_TYPE (int32_t, 4, le_to_i32p)
DECODE_SIMPLE_TYPE (uint32_t, 4, le_to_u32p)
#ifndef __KERNEL__
DECODE_SIMPLE_TYPE (int64_t, 8, le_to_i64p)
#endif
DECODE_SIMPLE_TYPE (uint64_t, 8, le_to_u64p)

#ifndef __KERNEL__
ENCODE_SIMPLE_TYPE (char, 1, )
#endif
ENCODE_SIMPLE_TYPE (uchar, 1, )
#ifndef __KERNEL__
ENCODE_SIMPLE_TYPE (int16_t, 2, i16_to_le)
ENCODE_SIMPLE_TYPE (uint16_t, 2, u16_to_le)
#endif
ENCODE_SIMPLE_TYPE (int32_t, 4, i32_to_le)
ENCODE_SIMPLE_TYPE (uint32_t, 4, u32_to_le)
#ifndef __KERNEL__
ENCODE_SIMPLE_TYPE (int64_t, 8, i64_to_le)
#endif
ENCODE_SIMPLE_TYPE (uint64_t, 8, u64_to_le)

bool
decode_data_buffer (DC *dc, data_buffer *data)
{
  if (!decode_uint32_t (dc, &data->len))
    return false;

  if (data->len > ZFS_MAXDATA)
    return false;

  dc->cur_length += data->len;
  if (dc->cur_length > dc->max_length)
    return false;

#ifdef __KERNEL__
  data->buf.k_buf = dc->cur_pos;
#else
  data->buf = dc->cur_pos;
#endif

  dc->cur_pos += data->len;

  return true;
}

bool
encode_data_buffer (DC *dc, data_buffer *data)
{
  unsigned int prev;

  if (!encode_uint32_t (dc, data->len))
    return false;

  prev = dc->cur_length;
  dc->cur_length += data->len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

#ifdef __KERNEL__
  if (data->user) {
    if (copy_from_user(dc->cur_pos, data->buf.u_rbuf, data->len)) {
          dc->cur_pos = NULL;
          return false;
        }
  } else {
        memcpy (dc->cur_pos, data->buf.k_buf, data->len);
  }
#else
  if (dc->cur_pos != data->buf)
    memcpy (dc->cur_pos, data->buf, data->len);
#endif
  dc->cur_pos += data->len;

  return true;
}

bool
decode_fixed_buffer (DC *dc, void *buf, int len)
{
  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    return false;

  memcpy (buf, dc->cur_pos, len);
  dc->cur_pos += len;

  return true;
}

bool
encode_fixed_buffer (DC *dc, void *buf, int len)
{
  unsigned int prev = dc->cur_length;

  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

  memcpy (dc->cur_pos, buf, len);
  dc->cur_pos += len;

  return true;
}

bool
decode_string (DC *dc, string *str, uint32_t max_len)
{
  if (!decode_uint32_t (dc, &str->len))
    return false;

  if (str->len > max_len)
    return false;

  dc->cur_length += str->len + 1;
  if (dc->cur_length > dc->max_length)
    return false;

  str->str = dc->cur_pos;
  str->str[str->len] = 0;
  dc->cur_pos += str->len + 1;

  return true;
}

bool
encode_string (DC *dc, string *str)
{
  unsigned int prev;

  if (!encode_uint32_t (dc, str->len))
    return false;

  prev = dc->cur_length;
  dc->cur_length += str->len + 1;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

  memcpy (dc->cur_pos, str->str, str->len + 1);
  dc->cur_pos += str->len + 1;

  return true;
}

#ifndef __KERNEL__

bool
decode_void (ATTRIBUTE_UNUSED DC *dc, ATTRIBUTE_UNUSED void *v)
{
  return true;
}

#endif

bool
encode_void (ATTRIBUTE_UNUSED DC *dc, ATTRIBUTE_UNUSED void *v)
{
  return true;
}

bool
decode_direction (DC *dc, direction *dir)
{
  uchar dir_val;
  bool r;

  r = decode_uchar (dc, &dir_val);
  if (r)
    {
      if (dir_val >= DIR_LAST_AND_UNUSED)
        r = false;
      else
        *dir = (direction) dir_val;
    }

  return r;
}

bool
encode_direction (DC *dc, direction dir)
{
  return encode_uchar (dc, (uchar) dir);
}

bool
decode_ftype (DC *dc, ftype *type)
{
  uchar type_val;
  bool r;

  r = decode_uchar (dc, &type_val);
  if (r)
    {
      if (type_val >= FT_LAST_AND_UNUSED)
        r = false;
      else
        *type = (ftype) type_val;
    }

  return r;
}

bool
encode_ftype (DC *dc, ftype type)
{
  return encode_uchar (dc, (uchar) type);
}

#ifndef __KERNEL__

bool
decode_connection_speed (DC *dc, connection_speed *speed)
{
  uchar speed_val;
  bool r;

  r = decode_uchar (dc, &speed_val);
  if (r)
    {
      if (speed_val >= CONNECTION_SPEED_LAST_AND_UNUSED)
        r = false;
      else
        *speed = (connection_speed) speed_val;
    }

  return r;
}

bool
encode_connection_speed (DC *dc, connection_speed speed)
{
  return encode_uchar (dc, (uchar) speed);
}

#endif

bool
decode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (decode_uint32_t (dc, &fh->sid)
          && decode_uint32_t (dc, &fh->vid)
          && decode_uint32_t (dc, &fh->dev)
          && decode_uint32_t (dc, &fh->ino)
          && decode_uint32_t (dc, &fh->gen));
}

bool
encode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (encode_uint32_t (dc, fh->sid)
          && encode_uint32_t (dc, fh->vid)
          && encode_uint32_t (dc, fh->dev)
          && encode_uint32_t (dc, fh->ino)
          && encode_uint32_t (dc, fh->gen));
}

bool
decode_zfs_cap (DC *dc, zfs_cap *cap)
{
  return (decode_zfs_fh (dc, &cap->fh)
          && decode_uint32_t (dc, &cap->flags)
          && decode_fixed_buffer (dc, &cap->verify, ZFS_VERIFY_LEN));
}

bool
encode_zfs_cap (DC *dc, zfs_cap *cap)
{
  return (encode_zfs_fh (dc, &cap->fh)
          && encode_uint32_t (dc, cap->flags)
          && encode_fixed_buffer (dc, &cap->verify, ZFS_VERIFY_LEN));
}

bool
decode_fattr (DC *dc, fattr *attr)
{
  return (decode_uint32_t (dc, &attr->dev)
          && decode_uint32_t (dc, &attr->ino)
          && decode_uint64_t (dc, &attr->version)
          && decode_ftype (dc, &attr->type)
          && decode_uint32_t (dc, &attr->mode)
          && decode_uint32_t (dc, &attr->nlink)
          && decode_uint32_t (dc, &attr->uid)
          && decode_uint32_t (dc, &attr->gid)
          && decode_uint32_t (dc, &attr->rdev)
          && decode_uint64_t (dc, &attr->size)
          && decode_uint64_t (dc, &attr->blocks)
          && decode_uint32_t (dc, &attr->blksize)
          && decode_zfs_time (dc, &attr->atime)
          && decode_zfs_time (dc, &attr->mtime)
          && decode_zfs_time (dc, &attr->ctime));
}

#ifndef __KERNEL__

bool
encode_fattr (DC *dc, fattr *attr)
{
  return (encode_uint32_t (dc, attr->dev)
          && encode_uint32_t (dc, attr->ino)
          && encode_uint64_t (dc, attr->version)
          && encode_ftype (dc, attr->type)
          && encode_uint32_t (dc, attr->mode)
          && encode_uint32_t (dc, attr->nlink)
          && encode_uint32_t (dc, attr->uid)
          && encode_uint32_t (dc, attr->gid)
          && encode_uint32_t (dc, attr->rdev)
          && encode_uint64_t (dc, attr->size)
          && encode_uint64_t (dc, attr->blocks)
          && encode_uint32_t (dc, attr->blksize)
          && encode_zfs_time (dc, &attr->atime)
          && encode_zfs_time (dc, &attr->mtime)
          && encode_zfs_time (dc, &attr->ctime));
}

bool
decode_sattr (DC *dc, sattr *attr)
{
  return (decode_uint32_t (dc, &attr->mode)
          && decode_uint32_t (dc, &attr->uid)
          && decode_uint32_t (dc, &attr->gid)
          && decode_uint64_t (dc, &attr->size)
          && decode_zfs_time (dc, &attr->atime)
          && decode_zfs_time (dc, &attr->mtime));
}

#endif

bool
encode_sattr (DC *dc, sattr *attr)
{
  return (encode_uint32_t (dc, attr->mode)
          && encode_uint32_t (dc, attr->uid)
          && encode_uint32_t (dc, attr->gid)
          && encode_uint64_t (dc, attr->size)
          && encode_zfs_time (dc, &attr->atime)
          && encode_zfs_time (dc, &attr->mtime));
}

bool
decode_filename (DC *dc, string *str)
{
  return decode_string (dc, str, ZFS_MAXNAMELEN);
}

bool
encode_filename (DC *dc, string *str)
{
  return encode_string (dc, str);
}

bool
decode_zfs_path (DC *dc, string *str)
{
  return decode_string (dc, str, ZFS_MAXPATHLEN);
}

bool
encode_zfs_path (DC *dc, string *str)
{
  return encode_string (dc, str);
}

#ifndef __KERNEL__

bool
decode_nodename (DC *dc, string *str)
{
  return decode_string (dc, str, ZFS_MAXNODELEN);
}

bool
encode_nodename (DC *dc, string *str)
{
  return encode_string (dc, str);
}

bool
decode_volume_root_args (DC *dc, volume_root_args *args)
{
  return decode_uint32_t (dc, &args->vid);
}

bool
encode_volume_root_args (DC *dc, volume_root_args *args)
{
  return encode_uint32_t (dc, args->vid);
}

bool
decode_setattr_args (DC *dc, setattr_args *args)
{
  return (decode_zfs_fh (dc, &args->file)
          && decode_sattr (dc, &args->attr));
}

#endif

bool
encode_setattr_args (DC *dc, setattr_args *args)
{
  return (encode_zfs_fh (dc, &args->file)
          && encode_sattr (dc, &args->attr));
}

#ifndef __KERNEL__

bool
decode_dir_op_args (DC *dc, dir_op_args *args)
{
  return (decode_zfs_fh (dc, &args->dir)
          && decode_filename (dc, &args->name));
}

#endif

bool
encode_dir_op_args (DC *dc, dir_op_args *args)
{
  return (encode_zfs_fh (dc, &args->dir)
          && encode_filename (dc, &args->name));
}

bool
decode_dir_op_res (DC *dc, dir_op_res *res)
{
  return (decode_zfs_fh (dc, &res->file)
          && decode_fattr (dc, &res->attr));
}

#ifndef __KERNEL__

bool
encode_dir_op_res (DC *dc, dir_op_res *res)
{
  return (encode_zfs_fh (dc, &res->file)
          && encode_fattr (dc, &res->attr));
}

bool
decode_create_args (DC *dc, create_args *args)
{
  return (decode_dir_op_args (dc, &args->where)
          && decode_uint32_t (dc, &args->flags)
          && decode_sattr (dc, &args->attr));
}

#endif

bool
encode_create_args (DC *dc, create_args *args)
{
  return (encode_dir_op_args (dc, &args->where)
          && encode_uint32_t (dc, args->flags)
          && encode_sattr (dc, &args->attr));
}

bool
decode_create_res (DC *dc, create_res *res)
{
  return (decode_zfs_cap (dc, &res->cap)
          && decode_zfs_fh (dc, &res->file)
          && decode_fattr (dc, &res->attr));
}

#ifndef __KERNEL__

bool
encode_create_res (DC *dc, create_res *res)
{
  return (encode_zfs_cap (dc, &res->cap)
          && encode_zfs_fh (dc, &res->file)
          && encode_fattr (dc, &res->attr));
}

bool
decode_open_args (DC *dc, open_args *args)
{
  return (decode_zfs_fh (dc, &args->file)
          && decode_uint32_t (dc, &args->flags));
}

#endif

bool
encode_open_args (DC *dc, open_args *args)
{
  return (encode_zfs_fh (dc, &args->file)
          && encode_uint32_t (dc, args->flags));
}

#ifndef __KERNEL__

bool
decode_read_dir_args (DC *dc, read_dir_args *args)
{
  return (decode_zfs_cap (dc, &args->cap)
          && decode_int32_t (dc, &args->cookie)
          && decode_uint32_t (dc, &args->count));
}

#endif

bool
encode_read_dir_args (DC *dc, read_dir_args *args)
{
  return (encode_zfs_cap (dc, &args->cap)
          && encode_int32_t (dc, args->cookie)
          && encode_uint32_t (dc, args->count));
}

bool
decode_dir_entry (DC *dc, dir_entry *entry)
{
  return (decode_uint32_t (dc, &entry->ino)
          && decode_int32_t (dc, &entry->cookie)
          && decode_filename (dc, &entry->name));
}

#ifndef __KERNEL__

bool
encode_dir_entry (DC *dc, dir_entry *entry)
{
  return (encode_uint32_t (dc, entry->ino)
          && encode_int32_t (dc, entry->cookie)
          && encode_filename (dc, &entry->name));
}

#endif

bool
decode_dir_list (DC *dc, dir_list *list)
{
  return (decode_uint32_t (dc, &list->n)
          && decode_char (dc, &list->eof));
}

#ifndef __KERNEL__

bool
encode_dir_list (DC *dc, dir_list *list)
{
  return (encode_uint32_t (dc, list->n)
          && encode_char (dc, list->eof));
}

bool
decode_mkdir_args (DC *dc, mkdir_args *args)
{
  return (decode_dir_op_args (dc, &args->where)
          && decode_sattr (dc, &args->attr));
}

#endif

bool
encode_mkdir_args (DC *dc, mkdir_args *args)
{
  return (encode_dir_op_args (dc, &args->where)
          && encode_sattr (dc, &args->attr));
}

#ifndef __KERNEL__

bool
decode_rename_args (DC *dc, rename_args *args)
{
  return (decode_dir_op_args (dc, &args->from)
          && decode_dir_op_args (dc, &args->to));
}

#endif

bool
encode_rename_args (DC *dc, rename_args *args)
{
  return (encode_dir_op_args (dc, &args->from)
          && encode_dir_op_args (dc, &args->to));
}

#ifndef __KERNEL__

bool
decode_link_args (DC *dc, link_args *args)
{
  return (decode_zfs_fh (dc, &args->from)
          && decode_dir_op_args (dc, &args->to));
}

#endif

bool
encode_link_args (DC *dc, link_args *args)
{
  return (encode_zfs_fh (dc, &args->from)
          && encode_dir_op_args (dc, &args->to));
}

#ifndef __KERNEL__

bool
decode_read_args (DC *dc, read_args *args)
{
  return (decode_zfs_cap (dc, &args->cap)
          && decode_uint64_t (dc, &args->offset)
          && decode_uint32_t (dc, &args->count));
}

#endif

bool
encode_read_args (DC *dc, read_args *args)
{
  return (encode_zfs_cap (dc, &args->cap)
          && encode_uint64_t (dc, args->offset)
          && encode_uint32_t (dc, args->count));
}

bool
decode_read_res (DC *dc, read_res *res)
{
  return (decode_data_buffer (dc, &res->data)
          && decode_uint64_t (dc, &res->version));
}

#ifndef __KERNEL__

bool
encode_read_res (DC *dc, read_res *res)
{
  return (encode_data_buffer (dc, &res->data)
          && encode_uint64_t (dc, res->version));
}

bool
decode_write_args (DC *dc, write_args *args)
{
  return (decode_zfs_cap (dc, &args->cap)
          && decode_uint64_t (dc, &args->offset)
          && decode_data_buffer (dc, &args->data));
}

#endif

bool
encode_write_args (DC *dc, write_args *args)
{
  return (encode_zfs_cap (dc, &args->cap)
          && encode_uint64_t (dc, args->offset)
          && encode_data_buffer (dc, &args->data));
}

bool
decode_write_res (DC *dc, write_res *res)
{
  return (decode_uint32_t (dc, &res->written)
          && decode_uint64_t (dc, &res->version));
}

#ifndef __KERNEL__

bool
encode_write_res (DC *dc, write_res *res)
{
  return (encode_uint32_t (dc, res->written)
          && encode_uint64_t (dc, res->version));
}

#endif

bool
decode_read_link_res (DC *dc, read_link_res *res)
{
  return decode_zfs_path (dc, &res->path);
}

#ifndef __KERNEL__

bool
encode_read_link_res (DC *dc, read_link_res *res)
{
  return encode_zfs_path (dc, &res->path);
}

bool
decode_symlink_args (DC *dc, symlink_args *args)
{
  return (decode_dir_op_args (dc, &args->from)
          && decode_zfs_path (dc, &args->to)
          && decode_sattr (dc, &args->attr));
}

#endif

bool
encode_symlink_args (DC *dc, symlink_args *args)
{
  return (encode_dir_op_args (dc, &args->from)
          && encode_zfs_path (dc, &args->to)
          && encode_sattr (dc, &args->attr));
}

#ifndef __KERNEL__

bool
decode_mknod_args (DC *dc, mknod_args *args)
{
  return (decode_dir_op_args (dc, &args->where)
          && decode_sattr (dc, &args->attr)
          && decode_ftype (dc, &args->type)
          && decode_uint32_t (dc, &args->rdev));
}

#endif

bool
encode_mknod_args (DC *dc, mknod_args *args)
{
  return (encode_dir_op_args (dc, &args->where)
          && encode_sattr (dc, &args->attr)
          && encode_ftype (dc, args->type)
          && encode_uint32_t (dc, args->rdev));
}

#ifndef __KERNEL__

bool
decode_auth_stage1_args (DC *dc, auth_stage1_args *args)
{
  return decode_nodename (dc, &args->node);
}

bool
encode_auth_stage1_args (DC *dc, auth_stage1_args *args)
{
  return encode_nodename (dc, &args->node);
}

bool
decode_auth_stage1_res (DC *dc, auth_stage1_res *res)
{
  return decode_nodename (dc, &res->node);
}

bool
encode_auth_stage1_res (DC *dc, auth_stage1_res *res)
{
  return encode_nodename (dc, &res->node);
}

bool
decode_auth_stage2_args (DC *dc, auth_stage2_args *args)
{
  return decode_connection_speed (dc, &args->speed);
}

bool
encode_auth_stage2_args (DC *dc, auth_stage2_args *args)
{
  return encode_connection_speed (dc, args->speed);
}

bool
decode_md5sum_args (DC *dc, md5sum_args *args)
{
  uint32_t i;

  if (!decode_zfs_cap (dc, &args->cap)
      || !decode_uint32_t (dc, &args->count)
      || !decode_char (dc, &args->ignore_changes))
    return false;

  if (args->count > ZFS_MAX_MD5_CHUNKS)
    return false;

  for (i = 0; i < args->count; i++)
    if (!decode_uint64_t (dc, &args->offset[i]))
      return false;

  for (i = 0; i < args->count; i++)
    if (!decode_uint32_t (dc, &args->length[i]))
      return false;

  return true;
}

bool
encode_md5sum_args (DC *dc, md5sum_args *args)
{
  uint32_t i;

#ifdef ENABLE_CHECKING
  if (args->count > ZFS_MAX_MD5_CHUNKS)
    abort ();
#endif

  encode_zfs_cap (dc, &args->cap);
  encode_uint32_t (dc, args->count);
  encode_char (dc, args->ignore_changes);

  for (i = 0; i < args->count; i++)
    encode_uint64_t (dc, args->offset[i]);

  for (i = 0; i < args->count; i++)
    encode_uint32_t (dc, args->length[i]);

  return true;
}

bool
decode_md5sum_res (DC *dc, md5sum_res *res)
{
  uint32_t i;

  if (!decode_uint32_t (dc, &res->count))
    return false;

  if (res->count > ZFS_MAX_MD5_CHUNKS)
    return false;

  if (!decode_uint64_t (dc, &res->size)
      || !decode_uint64_t (dc, &res->version))
    return false;

  for (i = 0; i < res->count; i++)
    if (!decode_uint64_t (dc, &res->offset[i]))
      return false;

  for (i = 0; i < res->count; i++)
    if (!decode_uint32_t (dc, &res->length[i]))
      return false;

  for (i = 0; i < res->count; i++)
    if (!decode_fixed_buffer (dc, res->md5sum[i], MD5_SIZE))
      return false;

  return true;
}

bool
encode_md5sum_res (DC *dc, md5sum_res *res)
{
  uint32_t i;

#ifdef ENABLE_CHECKING
  if (res->count > ZFS_MAX_MD5_CHUNKS)
    abort ();
#endif

  encode_uint32_t (dc, res->count);
  encode_uint64_t (dc, res->size);
  encode_uint64_t (dc, res->version);

  for (i = 0; i < res->count; i++)
    encode_uint64_t (dc, res->offset[i]);

  for (i = 0; i < res->count; i++)
    encode_uint32_t (dc, res->length[i]);

  for (i = 0; i < res->count; i++)
    encode_fixed_buffer (dc, res->md5sum[i], MD5_SIZE);

  return true;
}

bool
decode_file_info_res (DC *dc, file_info_res *res)
{
  return decode_zfs_path (dc, &res->path);
}

bool
encode_file_info_res (DC *dc, file_info_res *res)
{
  return encode_zfs_path (dc, &res->path);
}

bool
decode_reintegrate_args (DC *dc, reintegrate_args *args)
{
  return (decode_zfs_fh (dc, &args->fh)
          && decode_char (dc, &args->status));
}

bool
encode_reintegrate_args (DC *dc, reintegrate_args *args)
{
  return (encode_zfs_fh (dc, &args->fh)
          && encode_char (dc, args->status));
}

bool
decode_reintegrate_add_args (DC *dc, reintegrate_add_args *args)
{
  return (decode_zfs_fh (dc, &args->fh)
          && decode_zfs_fh (dc, &args->dir)
          && decode_filename (dc, &args->name));
}

bool
encode_reintegrate_add_args (DC *dc, reintegrate_add_args *args)
{
  return (encode_zfs_fh (dc, &args->fh)
          && encode_zfs_fh (dc, &args->dir)
          && encode_filename (dc, &args->name));
}

bool
decode_reintegrate_del_args (DC *dc, reintegrate_del_args *args)
{
  return (decode_zfs_fh (dc, &args->fh)
          && decode_zfs_fh (dc, &args->dir)
          && decode_filename (dc, &args->name)
          && decode_char (dc, &args->destroy_p));
}

bool
encode_reintegrate_del_args (DC *dc, reintegrate_del_args *args)
{
  return (encode_zfs_fh (dc, &args->fh)
          && encode_zfs_fh (dc, &args->dir)
          && encode_filename (dc, &args->name)
          && encode_char (dc, args->destroy_p));
}

bool
decode_reintegrate_ver_args (DC *dc, reintegrate_ver_args *args)
{
  return (decode_zfs_fh (dc, &args->fh)
          && decode_uint64_t (dc, &args->version_inc));
}

bool
encode_reintegrate_ver_args (DC *dc, reintegrate_ver_args *args)
{
  return (encode_zfs_fh (dc, &args->fh)
          && encode_uint64_t (dc, args->version_inc));
}

#endif

#ifdef __KERNEL__
bool
decode_invalidate_args (DC *dc, invalidate_args *args)
{
  return decode_zfs_fh (dc, &args->fh);
}

#else

bool
encode_invalidate_args (DC *dc, invalidate_args *args)
{
  return encode_zfs_fh (dc, &args->fh);
}

bool
decode_reread_config_args (DC *dc, reread_config_args *args)
{
  return decode_zfs_path (dc, &args->path);
}

bool
encode_reread_config_args (DC *dc, reread_config_args *args)
{
  return encode_zfs_path (dc, &args->path);
}
#endif
