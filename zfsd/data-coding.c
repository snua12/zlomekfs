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

/* Allocate a new data coding buffer of size SIZE and fill information
   about it in DC.  */

void
dc_create (DC *dc, unsigned int size)
{
  dc->unaligned = (char *) xmalloc (size + 15);
  dc->buffer = (char *) ALIGN_PTR_16 (dc->unaligned);
  dc->size = size;
#ifdef ENABLE_CHECKING
  if (size < 0)
    abort ();
#endif
}

/* Free the data coding buffer DC.  */

void
dc_destroy (DC *dc)
{
  free (dc->unaligned);
}

/* Initialize DC to start encoding to PTR with maximal length MAX_LENGTH.  */

void
start_encoding (DC *dc)
{
  dc->current = dc->buffer;
  dc->cur_length = 0;
  dc->max_length = dc->size;
  encode_uint32_t (dc, 0);
}

/* Update the size of block in DC.  Return the length of encoded buffer.  */

int
finish_encoding (DC *dc)
{
  *(uint32_t *) dc->buffer = u32_to_le ((uint32_t) dc->cur_length);

  return dc->cur_length;
}

/* Initialize DC to start decoding of PTR.  Return true on success.  */

bool
start_decoding (DC *dc)
{
  dc->current = dc->buffer;
  dc->max_length = 4;
  dc->cur_length = 0;
  decode_uint32_t (dc, (uint32_t *) &dc->max_length);
  return dc->max_length <= dc->size;
}

/* Return true if all data has been read from encoded buffer.  */

bool
finish_decoding (DC *dc)
{
  return dc->cur_length == dc->max_length;
}

/* Decode a value of type T and size S from DC and store it to *RET.
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
  dc->current = (char *) ALIGN_PTR_##S (dc->current);		\
  *ret = F (dc->current);					\
  dc->current += S;						\
								\
  return true;							\
}

/* Encode a value VAL of type T and size S to DC.
   Call F to transform cpu endian to little endian.
   Return true on success.  */
#define ENCODE_SIMPLE_TYPE(T, S, F)				\
bool								\
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
      return false;						\
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
  return true;							\
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

  memcpy (data->buf, dc->current, data->len);
  dc->current += data->len;

  return true;
}

bool
encode_data_buffer (DC *dc, data_buffer *data)
{
  int prev;

  if (!encode_uint32_t (dc, data->len))
    return false;

  prev = dc->cur_length;
  dc->cur_length += data->len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

  memcpy (dc->current, data->buf, data->len);
  dc->current += data->len;

  return true;
}

bool
decode_fixed_buffer (DC *dc, void *buf, int len)
{
  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    return false;

  memcpy (buf, dc->current, len);
  dc->current += len;

  return true;
}

bool
encode_fixed_buffer (DC *dc, void *buf, int len)
{
  int prev = dc->cur_length;

  dc->cur_length += len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

  memcpy (dc->current, buf, len);
  dc->current += len;

  return true;
}

bool
decode_string (DC *dc, string *str, uint32_t max_len)
{
  if (!decode_uint32_t (dc, &str->len))
    return false;

  if (str->len > max_len)
    return false;

  dc->cur_length += str->len;
  if (dc->cur_length > dc->max_length)
    return false;

  str->str = (char *) xmalloc (str->len + 1);
  memcpy (str->str, dc->current, str->len);
  str->str[str->len] = 0;
  dc->current += str->len;

  return true;
}

bool
encode_string (DC *dc, string *str)
{
  int prev;

  if (!encode_uint32_t (dc, str->len))
    return false;

  prev = dc->cur_length;
  dc->cur_length += str->len;
  if (dc->cur_length > dc->max_length)
    {
      dc->cur_length = prev;
      return false;
    }

  memcpy (dc->current, str->str, str->len);
  dc->current += str->len;

  return true;
}

bool
decode_void (DC *dc, void *v)
{
  return true;
}

bool
encode_void (DC *dc, void *v)
{
  return true;
}

bool
decode_direction (DC *dc, direction *dir)
{
  uchar dir_val;
  int r;

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
  int r;

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

bool
decode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (decode_uint32_t (dc, &fh->sid)
	  && decode_uint32_t (dc, &fh->vid)
	  && decode_uint32_t (dc, &fh->dev)
	  && decode_uint32_t (dc, &fh->ino));
}

bool
encode_zfs_fh (DC *dc, zfs_fh *fh)
{
  return (encode_uint32_t (dc, fh->sid)
	  && encode_uint32_t (dc, fh->vid)
	  && encode_uint32_t (dc, fh->dev)
	  && encode_uint32_t (dc, fh->ino));
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
	  && decode_uint32_t (dc, &attr->dev)
	  && decode_uint32_t (dc, &attr->ino)
	  && decode_zfs_time (dc, &attr->atime)
	  && decode_zfs_time (dc, &attr->mtime)
	  && decode_zfs_time (dc, &attr->ctime));
}

bool
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
	  && encode_uint32_t (dc, attr->dev)
	  && encode_uint32_t (dc, attr->ino)
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
decode_sattr_args (DC *dc, sattr_args *args)
{
  return (decode_zfs_fh (dc, &args->file)
	  && decode_sattr (dc, &args->attr));
}

bool
encode_sattr_args (DC *dc, sattr_args *args)
{
  return (encode_zfs_fh (dc, &args->file)
	  && encode_sattr (dc, &args->attr));
}

bool
decode_dir_op_args (DC *dc, dir_op_args *args)
{
  return (decode_zfs_fh (dc, &args->dir)
	  && decode_filename (dc, &args->name));
}

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

bool
encode_dir_op_res (DC *dc, dir_op_res *res)
{
  return (encode_zfs_fh (dc, &res->file)
	  && encode_fattr (dc, &res->attr));
}

bool
decode_open_name_args (DC *dc, open_name_args *args)
{
  return (decode_dir_op_args (dc, &args->where)
	  && decode_sattr (dc, &args->attr));
}

bool
encode_open_name_args (DC *dc, open_name_args *args)
{
  return (encode_dir_op_args (dc, &args->where)
	  && encode_sattr (dc, &args->attr));
}

bool
decode_open_fh_args (DC *dc, open_fh_args *args)
{
  return (decode_zfs_fh (dc, &args->file)
	  && decode_uint32_t (dc, &args->flags));
}

bool
encode_open_fh_args (DC *dc, open_fh_args *args)
{
  return (encode_zfs_fh (dc, &args->file)
	  && encode_uint32_t (dc, args->flags));
}

bool
decode_read_dir_args (DC *dc, read_dir_args *args)
{
  return (decode_zfs_cap (dc, &args->dir)
	  && decode_int32_t (dc, &args->cookie)
	  && decode_uint32_t (dc, &args->count));
}

bool
encode_read_dir_args (DC *dc, read_dir_args *args)
{
  return (encode_zfs_cap (dc, &args->dir)
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

bool
encode_dir_entry (DC *dc, dir_entry *entry)
{
  return (encode_uint32_t (dc, entry->ino)
	  && encode_int32_t (dc, entry->cookie)
	  && encode_filename (dc, &entry->name));
}

bool
decode_dir_list (DC *dc, dir_list *list)
{
  return (decode_uint32_t (dc, &list->n)
	  && decode_char (dc, &list->eof));
}

bool
encode_dir_list (DC *dc, dir_list *list)
{
  return (encode_uint32_t (dc, list->n)
	  && encode_char (dc, list->eof));
}

bool
decode_rename_args (DC *dc, rename_args *args)
{
  return (decode_dir_op_args (dc, &args->from)
	  && decode_dir_op_args (dc, &args->to));
}

bool
encode_rename_args (DC *dc, rename_args *args)
{
  return (encode_dir_op_args (dc, &args->from)
	  && encode_dir_op_args (dc, &args->to));
}

bool
decode_link_args (DC *dc, link_args *args)
{
  return (decode_zfs_fh (dc, &args->from)
	  && decode_dir_op_args (dc, &args->to));
}

bool
encode_link_args (DC *dc, link_args *args)
{
  return (decode_zfs_fh (dc, &args->from)
	  && decode_dir_op_args (dc, &args->to));
}

bool
decode_read_args (DC *dc, read_args *args)
{
  return (decode_zfs_cap (dc, &args->file)
	  && decode_uint64_t (dc, &args->offset)
	  && decode_uint32_t (dc, &args->count));
}

bool
encode_read_args (DC *dc, read_args *args)
{
  return (encode_zfs_cap (dc, &args->file)
	  && encode_uint64_t (dc, args->offset)
	  && encode_uint32_t (dc, args->count));
}

bool
decode_read_res (DC *dc, read_res *res)
{
  return (decode_data_buffer (dc, &res->data));
}

bool
encode_read_res (DC *dc, read_res *res)
{
  return (encode_data_buffer (dc, &res->data));
}

bool
decode_write_args (DC *dc, write_args *args)
{
  return (decode_zfs_cap (dc, &args->file)
	  && decode_uint64_t (dc, &args->offset)
	  && decode_data_buffer (dc, &args->data));
}

bool
encode_write_args (DC *dc, write_args *args)
{
  return (encode_zfs_cap (dc, &args->file)
	  && encode_uint64_t (dc, args->offset)
	  && encode_data_buffer (dc, &args->data));
}

bool
decode_write_res (DC *dc, write_res *res)
{
  return decode_uint32_t (dc, &res->written);
}

bool
encode_write_res (DC *dc, write_res *res)
{
  return encode_uint32_t (dc, res->written);
}

bool
decode_read_link_res (DC *dc, read_link_res *res)
{
  return decode_zfs_path (dc, &res->path);
}

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

bool
encode_symlink_args (DC *dc, symlink_args *args)
{
  return (encode_dir_op_args (dc, &args->from)
	  && encode_zfs_path (dc, &args->to)
	  && encode_sattr (dc, &args->attr));
}

bool
decode_mknod_args (DC *dc, mknod_args *args)
{
  return (decode_dir_op_args (dc, &args->where)
	  && decode_ftype (dc, &args->type)
	  && decode_sattr (dc, &args->attr)
	  && decode_uint32_t (dc, &args->rdev));
}

bool
encode_mknod_args (DC *dc, mknod_args *args)
{
  return (encode_dir_op_args (dc, &args->where)
	  && encode_ftype (dc, args->type)
	  && encode_sattr (dc, &args->attr)
	  && encode_uint32_t (dc, args->rdev));
}

bool
decode_auth_stage1_args (DC *dc, auth_stage1_args *args)
{
  return (decode_fixed_buffer (dc, &args->auth, ZFS_AUTH_LEN)
	  && decode_nodename (dc, &args->node));
}

bool
encode_auth_stage1_args (DC *dc, auth_stage1_args *args)
{
  return (encode_fixed_buffer (dc, &args->auth, ZFS_AUTH_LEN)
	  && encode_nodename (dc, &args->node));
}

bool
decode_auth_stage2_args (DC *dc, auth_stage2_args *args)
{
  return decode_fixed_buffer (dc, &args->auth, ZFS_AUTH_LEN);
}

bool
encode_auth_stage2_args (DC *dc, auth_stage2_args *args)
{
  return encode_fixed_buffer (dc, &args->auth, ZFS_AUTH_LEN);
}
