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

#ifndef DATA_CODING_H
#define DATA_CODING_H

#include "system.h"
#include <inttypes.h>
#include <netinet/in.h>
#include "zfs_prot.h"

/* Align a number to be a multiple of 2 ,4, 8, 16.  */
#define ALIGN_1(N) (N)
#define ALIGN_2(N) (((N) + 1) & ~1)
#define ALIGN_4(N) (((N) + 3) & ~3)
#define ALIGN_8(N) (((N) + 7) & ~7)
#define ALIGN_16(N) (((N) + 15) & ~15)

/* Align a pointer to be a multiple of 2 ,4, 8, 16.  */
#define ALIGN_PTR_1(P) ((void *) ALIGN_1 ((uintptr_t) (P)))
#define ALIGN_PTR_2(P) ((void *) ALIGN_2 ((uintptr_t) (P)))
#define ALIGN_PTR_4(P) ((void *) ALIGN_4 ((uintptr_t) (P)))
#define ALIGN_PTR_8(P) ((void *) ALIGN_8 ((uintptr_t) (P)))
#define ALIGN_PTR_16(P) ((void *) ALIGN_16 ((uintptr_t) (P)))

/* Extract the I-th byte of the number.  */
#define byte0(X) ((unsigned char) (X))
#define byte1(X) ((unsigned char) ((X) >> 8))
#define byte2(X) ((unsigned char) ((X) >> 16))
#define byte3(X) ((unsigned char) ((X) >> 24))
#define byte4(X) ((unsigned char) ((X) >> 32))
#define byte5(X) ((unsigned char) ((X) >> 40))
#define byte6(X) ((unsigned char) ((X) >> 48))
#define byte7(X) ((unsigned char) ((X) >> 56))

/* Define the byte order conversion macros.  */
#ifndef BYTE_ORDER

#error BYTE_ORDER is not defined

#elif BYTE_ORDER == LITTLE_ENDIAN

#define i16_to_le(X) ((int16_t) X)
#define u16_to_le(X) ((uint16_t) X)
#define i32_to_le(X) ((int32_t) X)
#define u32_to_le(X) ((uint32_t) X)
#define i64_to_le(X) ((int64_t) X)
#define u64_to_le(X) ((uint64_t) X)
#define le_to_i16(X) ((int16_t) X)
#define le_to_u16(X) ((uint16_t) X)
#define le_to_i32(X) ((int32_t) X)
#define le_to_u32(X) ((uint32_t) X)
#define le_to_i64(X) ((int64_t) X)
#define le_to_u64(X) ((uint64_t) X)

#elif BYTE_ORDER == BIG_ENDIAN

#define i16_to_le(X)							     \
  ((int16_t) byte0 (X) << 8 | byte1 (X))
#define u16_to_le(X)							     \
  ((uint16_t) byte0 (X) << 8 | byte1 (X))
#define i32_to_le(X)							     \
  ((int32_t) byte0 (X) << 24 | byte1 (X) << 16 | byte2 (X) << 8 | byte3 (X))
#define u32_to_le(X)							     \
  ((uint32_t) byte0 (X) << 24 | byte1 (X) << 16 | byte2 (X) << 8 | byte3 (X))
#define i64_to_le(X)							     \
  ((int64_t) byte4 (X) << 24 | byte5 (X) << 16 | byte6 (X) << 8 | byte7 (X)  \
   | byte0 (X) << 56 | byte1 (X) << 48 | byte2 (X) << 40 | byte3 (X) << 32)
#define u64_to_le(X)							     \
  ((uint64_t) byte4 (X) << 24 | byte5 (X) << 16 | byte6 (X) << 8 | byte7 (X) \
   | byte0 (X) << 56 | byte1 (X) << 48 | byte2 (X) << 40 | byte3 (X) << 32)
#define le_to_i16(X)							     \
  ((int16_t) byte0 (X) << 8 | byte1 (X))
#define le_to_u16(X)							     \
  ((uint16_t) byte0 (X) << 8 | byte1 (X))
#define le_to_i32(X)							     \
  ((int32_t) byte0 (X) << 24 | byte1 (X) << 16 | byte2 (X) << 8 | byte3 (X))
#define le_to_u32(X)							     \
  ((uint32_t) byte0 (X) << 24 | byte1 (X) << 16 | byte2 (X) << 8 | byte3 (X))
#define le_to_i64(X)							     \
  ((int64_t) byte4 (X) << 24 | byte5 (X) << 16 | byte6 (X) << 8 | byte7 (X)  \
   | byte0 (X) << 56 | byte1 (X) << 48 | byte2 (X) << 40 | byte3 (X) << 32)
#define le_to_u64(X)							     \
  ((uint64_t) byte4 (X) << 24 | byte5 (X) << 16 | byte6 (X) << 8 | byte7 (X) \
   | byte0 (X) << 56 | byte1 (X) << 48 | byte2 (X) << 40 | byte3 (X) << 32)

#elif BYTE_ORDER == PDP_ENDIAN

#error PDP_ENDIAN is not supported

#else

#error BYTE_ORDER is not defined correctly

#endif

#define i16_to_lep(X) i16_to_le (*(int16_t *) (X))
#define u16_to_lep(X) u16_to_le (*(uint16_t *) (X))
#define i32_to_lep(X) i32_to_le (*(int32_t *) (X))
#define u32_to_lep(X) u32_to_le (*(uint32_t *) (X))
#define i64_to_lep(X) i64_to_le (*(int64_t *) (X))
#define u64_to_lep(X) u64_to_le (*(uint64_t *) (X))
#define le_to_i16p(X) le_to_i16 (*(int16_t *) (X))
#define le_to_u16p(X) le_to_u16 (*(uint16_t *) (X))
#define le_to_i32p(X) le_to_i32 (*(int32_t *) (X))
#define le_to_u32p(X) le_to_u32 (*(uint32_t *) (X))
#define le_to_i64p(X) le_to_i64 (*(int64_t *) (X))
#define le_to_u64p(X) le_to_u64 (*(uint64_t *) (X))

/* Get an unsigned int (32-bit) from the pointer and perform little/big
   endian conversion.  */
#define GET_UINT(P) (le_to_u32p (P))

typedef struct data_coding_def
{
  char *original;
  char *current;
  int max_length;
  int cur_length;
} DC;

extern void start_encoding (DC *dc, void *ptr, int max_length);
extern int finish_encoding (DC *dc);
extern void start_decoding (void *ptr);

extern int decode_char (DC *dc, char *ret);
extern int decode_int16_t (DC *dc, int16_t *ret);
extern int decode_uint16_t (DC *dc, uint16_t *ret);
extern int decode_int32_t (DC *dc, int32_t *ret);
extern int decode_uint32_t (DC *dc, uint32_t *ret);
extern int decode_int64_t (DC *dc, int64_t *ret);
extern int decode_uint64_t (DC *dc, uint64_t *ret);

extern int encode_char (DC *dc, char val);
extern int encode_int16_t (DC *dc, int16_t val);
extern int encode_uint16_t (DC *dc, uint16_t val);
extern int encode_int32_t (DC *dc, int32_t val);
extern int encode_uint32_t (DC *dc, uint32_t val);
extern int encode_int64_t (DC *dc, int64_t val);
extern int encode_uint64_t (DC *dc, uint64_t val);

extern int decode_data_buffer (DC *dc, data_buffer *data);
extern int encode_data_buffer (DC *dc, data_buffer *data);
extern int decode_string (DC *dc, string *str, uint32_t max_len);
extern int encode_string (DC *dc, string *str);
extern int decode_ftype (DC *dc, ftype *type);
extern int encode_ftype (DC *dc, ftype type);
extern int decode_zfs_fh (DC *dc, zfs_fh *fh);
extern int encode_zfs_fh (DC *dc, zfs_fh *fh);
extern int decode_zfs_time (DC *dc, zfs_time *time);
extern int encode_zfs_time (DC *dc, zfs_time *time);
extern int decode_fattr (DC *dc, fattr *attr);
extern int encode_fattr (DC *dc, fattr *attr);
extern int decode_sattr (DC *dc, sattr *attr);
extern int encode_sattr (DC *dc, sattr *attr);
extern int decode_filename (DC *dc, string *str);
extern int encode_filename (DC *dc, string *str);
extern int decode_zfs_path (DC *dc, string *str);
extern int encode_zfs_path (DC *dc, string *str);
extern int decode_sattr_args (DC *dc, sattr_args *args);
extern int encode_sattr_args (DC *dc, sattr_args *args);
extern int decode_dir_op_args (DC *dc, dir_op_args *args);
extern int encode_dir_op_args (DC *dc, dir_op_args *args);
extern int decode_dir_op_res (DC *dc, dir_op_res *res);
extern int encode_dir_op_res (DC *dc, dir_op_res *res);
extern int decode_open_name_args (DC *dc, open_name_args *args);
extern int encode_open_name_args (DC *dc, open_name_args *args);
extern int decode_open_res (DC *dc, open_res *res);
extern int encode_open_res (DC *dc, open_res *res);
extern int decode_read_dir_args (DC *dc, read_dir_args *args);
extern int encode_read_dir_args (DC *dc, read_dir_args *args);
/* FIXME: reply of readdir */
extern int decode_rename_args (DC *dc, rename_args *args);
extern int encode_rename_args (DC *dc, rename_args *args);
extern int decode_link_args (DC *dc, link_args *args);
extern int encode_link_args (DC *dc, link_args *args);
extern int decode_read_args (DC *dc, read_args *args);
extern int encode_read_args (DC *dc, read_args *args);
extern int decode_read_res (DC *dc, read_res *res);
extern int encode_read_res (DC *dc, read_res *res);
extern int decode_write_args (DC *dc, write_args *args);
extern int encode_write_args (DC *dc, write_args *args);
extern int decode_write_res (DC *dc, write_res *res);
extern int encode_write_res (DC *dc, write_res *res);
extern int decode_read_link_res (DC *dc, read_link_res *res);
extern int encode_read_link_res (DC *dc, read_link_res *res);
extern int decode_symlink_args (DC *dc, symlink_args *args);
extern int encode_symlink_args (DC *dc, symlink_args *args);
extern int decode_mknod_args (DC *dc, mknod_args *args);
extern int encode_mknod_args (DC *dc, mknod_args *args);

#endif
