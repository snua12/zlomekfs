/* Data coding functions (encoding and decoding requests and replies).
   Copyright (C) 2003, 2004 Josef Zlomek

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

#ifdef __KERNEL__
# include <linux/types.h>
typedef unsigned int		uintptr_t;
#else
# include <inttypes.h>
# include <netinet/in.h>
# include <stdio.h>
#endif

/* Maximal length of request / reply.  */
#define DC_SIZE 8888

/* Align a number to be a multiple of 2, 4, 8, 16, 256.  */
#define ALIGN_1(N) (N)
#define ALIGN_2(N) (((N) + 1) & ~1)
#define ALIGN_4(N) (((N) + 3) & ~3)
#define ALIGN_8(N) (((N) + 7) & ~7)
#define ALIGN_16(N) (((N) + 15) & ~15)
#define ALIGN_256(N) (((N) + 255) & ~255)

/* Align a pointer to be a multiple of 2, 4, 8, 16, 256.  */
#define ALIGN_PTR_1(P) ((void *) ALIGN_1 ((uintptr_t) (P)))
#define ALIGN_PTR_2(P) ((void *) ALIGN_2 ((uintptr_t) (P)))
#define ALIGN_PTR_4(P) ((void *) ALIGN_4 ((uintptr_t) (P)))
#define ALIGN_PTR_8(P) ((void *) ALIGN_8 ((uintptr_t) (P)))
#define ALIGN_PTR_16(P) ((void *) ALIGN_16 ((uintptr_t) (P)))
#define ALIGN_PTR_256(P) ((void *) ALIGN_256 ((uintptr_t) (P)))

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

typedef struct data_coding_def
{
  char *buffer;			/* previous pointer aligned to 16 */
  char *cur_pos;		/* current position to buffer while
				   encoding/decoding */
  unsigned int max_length;	/* maximal valid index to buffer */ 
  unsigned int cur_length;	/* current index to buffer */
  char data[DC_SIZE + 15];
} DC;

#include "zfs_prot.h"

extern DC *dc_create (void);
extern void dc_destroy (DC *dc);
#ifndef __KERNEL__
extern void print_dc (DC *dc, FILE *f);
extern void debug_dc (DC *dc);
#endif
extern void start_encoding (DC *dc);
extern unsigned int finish_encoding (DC *dc);
extern bool start_decoding (DC *dc);
extern bool finish_decoding (DC *dc);

typedef unsigned char uchar;

extern bool decode_char (DC *dc, char *ret);
extern bool decode_uchar (DC *dc, uchar *ret);
extern bool decode_int16_t (DC *dc, int16_t *ret);
extern bool decode_uint16_t (DC *dc, uint16_t *ret);
extern bool decode_int32_t (DC *dc, int32_t *ret);
extern bool decode_uint32_t (DC *dc, uint32_t *ret);
extern bool decode_int64_t (DC *dc, int64_t *ret);
extern bool decode_uint64_t (DC *dc, uint64_t *ret);

extern bool encode_char (DC *dc, char val);
extern bool encode_uchar (DC *dc, uchar val);
extern bool encode_int16_t (DC *dc, int16_t val);
extern bool encode_uint16_t (DC *dc, uint16_t val);
extern bool encode_int32_t (DC *dc, int32_t val);
extern bool encode_uint32_t (DC *dc, uint32_t val);
extern bool encode_int64_t (DC *dc, int64_t val);
extern bool encode_uint64_t (DC *dc, uint64_t val);

#define decode_length(DC, L) decode_uint32_t ((DC), (L))
#define encode_length(DC, L) encode_uint32_t ((DC), (L))
#define decode_request_id(DC, L) decode_uint32_t ((DC), (L))
#define encode_request_id(DC, L) encode_uint32_t ((DC), (L))
#define decode_status(DC, L) decode_int32_t ((DC), (L))
#define encode_status(DC, L) encode_int32_t ((DC), (L))
#define decode_function(DC, L) decode_uint32_t ((DC), (L))
#define encode_function(DC, L) encode_uint32_t ((DC), (L))

#define decode_zfs_time(DC, T) decode_uint32_t ((DC), (T))
#define encode_zfs_time(DC, T) encode_uint32_t ((DC), *(T))

extern bool decode_data_buffer (DC *dc, data_buffer *data);
extern bool encode_data_buffer (DC *dc, data_buffer *data);
extern bool decode_fixed_buffer (DC *dc, void *buf, int len);
extern bool encode_fixed_buffer (DC *dc, void *buf, int len);
extern bool decode_string (DC *dc, string *str, uint32_t max_len);
extern bool encode_string (DC *dc, string *str);
extern bool decode_void (DC *dc, void *v);
extern bool encode_void (DC *dc, void *v);
extern bool decode_direction (DC *dc, direction *dir);
extern bool encode_direction (DC *dc, direction dir);
extern bool decode_ftype (DC *dc, ftype *type);
extern bool encode_ftype (DC *dc, ftype type);
extern bool decode_zfs_fh (DC *dc, zfs_fh *fh);
extern bool encode_zfs_fh (DC *dc, zfs_fh *fh);
extern bool decode_zfs_cap (DC *dc, zfs_cap *cap);
extern bool encode_zfs_cap (DC *dc, zfs_cap *cap);
extern bool decode_fattr (DC *dc, fattr *attr);
extern bool encode_fattr (DC *dc, fattr *attr);
extern bool decode_sattr (DC *dc, sattr *attr);
extern bool encode_sattr (DC *dc, sattr *attr);
extern bool decode_filename (DC *dc, string *str);
extern bool encode_filename (DC *dc, string *str);
extern bool decode_zfs_path (DC *dc, string *str);
extern bool encode_zfs_path (DC *dc, string *str);
extern bool decode_nodename (DC *dc, string *str);
extern bool encode_nodename (DC *dc, string *str);
extern bool decode_volume_root_args (DC *dc, volume_root_args *args);
extern bool encode_volume_root_args (DC *dc, volume_root_args *args);
extern bool decode_sattr_args (DC *dc, sattr_args *args);
extern bool encode_sattr_args (DC *dc, sattr_args *args);
extern bool decode_dir_op_args (DC *dc, dir_op_args *args);
extern bool encode_dir_op_args (DC *dc, dir_op_args *args);
extern bool decode_dir_op_res (DC *dc, dir_op_res *res);
extern bool encode_dir_op_res (DC *dc, dir_op_res *res);
extern bool decode_create_args (DC *dc, create_args *args);
extern bool encode_create_args (DC *dc, create_args *args);
extern bool decode_create_res (DC *dc, create_res *res);
extern bool encode_create_res (DC *dc, create_res *res);
extern bool decode_open_args (DC *dc, open_args *args);
extern bool encode_open_args (DC *dc, open_args *args);
extern bool decode_read_dir_args (DC *dc, read_dir_args *args);
extern bool encode_read_dir_args (DC *dc, read_dir_args *args);
extern bool decode_dir_entry (DC *dc, dir_entry *entry);
extern bool encode_dir_entry (DC *dc, dir_entry *entry);
extern bool decode_dir_list (DC *dc, dir_list *list);
extern bool encode_dir_list (DC *dc, dir_list *list);
extern bool decode_mkdir_args (DC *dc, mkdir_args *args);
extern bool encode_mkdir_args (DC *dc, mkdir_args *args);
extern bool decode_rename_args (DC *dc, rename_args *args);
extern bool encode_rename_args (DC *dc, rename_args *args);
extern bool decode_link_args (DC *dc, link_args *args);
extern bool encode_link_args (DC *dc, link_args *args);
extern bool decode_read_args (DC *dc, read_args *args);
extern bool encode_read_args (DC *dc, read_args *args);
extern bool decode_write_args (DC *dc, write_args *args);
extern bool encode_write_args (DC *dc, write_args *args);
extern bool decode_write_res (DC *dc, write_res *res);
extern bool encode_write_res (DC *dc, write_res *res);
extern bool decode_read_link_res (DC *dc, read_link_res *res);
extern bool encode_read_link_res (DC *dc, read_link_res *res);
extern bool decode_symlink_args (DC *dc, symlink_args *args);
extern bool encode_symlink_args (DC *dc, symlink_args *args);
extern bool decode_mknod_args (DC *dc, mknod_args *args);
extern bool encode_mknod_args (DC *dc, mknod_args *args);
extern bool decode_auth_stage1_args (DC *dc, auth_stage1_args *args);
extern bool encode_auth_stage1_args (DC *dc, auth_stage1_args *args);
extern bool decode_auth_stage2_args (DC *dc, auth_stage2_args *args);
extern bool encode_auth_stage2_args (DC *dc, auth_stage2_args *args);
extern bool decode_md5sum_args (DC *dc, md5sum_args *args);
extern bool encode_md5sum_args (DC *dc, md5sum_args *args);
extern bool decode_md5sum_res (DC *dc, md5sum_res *res);
extern bool encode_md5sum_res (DC *dc, md5sum_res *res);
extern bool decode_hardlinks_args (DC *dc, hardlinks_args *args);
extern bool encode_hardlinks_args (DC *dc, hardlinks_args *args);
extern bool decode_hardlinks_res (DC *dc, hardlinks_res *res);
extern bool encode_hardlinks_res (DC *dc, hardlinks_res *res);

#endif
