/* packet-zfsd.c
 * Routines for zlomek fs protocol dissection
 * Copyright 2010, Ales Snuparek <snuparek@gmail.com>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Copied from WHATEVER_FILE_YOU_USED (where "WHATEVER_FILE_YOU_USED"
 * is a dissector file; if you just copied this from README.developer,
 * don't bother with the "Copied from" - you don't even need to put
 * in a "Copied from" if you copied an existing dissector, especially
 * if the bulk of the code in the new dissector is your code)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if 0
/* Include only as needed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <glib.h>

#include <epan/packet.h>
#include <epan/prefs.h>
#include "../../epan/dissectors/packet-tcp.h"

/*! Align a number to be a multiple of 2, 4, 8, 16, 256.  */
#define ALIGN_1(N) (N)
#define ALIGN_2(N) (((N) + 1) & ~1)
#define ALIGN_4(N) (((N) + 3) & ~3)
#define ALIGN_8(N) (((N) + 7) & ~7)
#define ALIGN_16(N) (((N) + 15) & ~15)
#define ALIGN_256(N) (((N) + 255) & ~255)

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif


#define ZFS_MESSAGE_LEN_MIN (4 + 4 + 4)
#define DC_SIZE (8888)
/*value is based on DC_SIZE + 15 size of DataCoding buffer*/
#define ZFS_MESSAGE_LEN_MAX (DC_SIZE + 15)

typedef enum direction_def
{
	DIR_REQUEST,          /*!< Request which wants a reply.  */
	DIR_REPLY,            /*!< Reply to request.  */
	DIR_ONEWAY,           /*!< Request which does not want a reply.  */
	DIR_LAST_AND_UNUSED       
} direction;


static const value_string packettypenames[] ={
	{DIR_REQUEST, "DIR_REQUEST"},
	{DIR_REPLY, "DIR_REPLY"},
	{DIR_ONEWAY, "DIR_ONEWAY"},
	{DIR_LAST_AND_UNUSED, "DIR_LAST_AND_UNUSED"}
};

enum function_number_def
{
#define ZFS_CALL_CLIENT
#define ZFS_CALL_SERVER
#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)  \
	  ZFS_PROC_##NAME = NUMBER,
#include "zfs-prot.def"
	    ZFS_PROC_LAST_AND_UNUSED
};
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL
#undef ZFS_CALL_SERVER
#undef ZFS_CALL_CLIENT

static const value_string packetfunctionnames[] ={
#define ZFS_CALL_CLIENT
#define ZFS_CALL_SERVER
#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)  \
	  {ZFS_PROC_##NAME, STR(NAME)},
#include "zfs-prot.def"
	  {0, NULL}
};
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL
#undef ZFS_CALL_SERVER
#undef ZFS_CALL_CLIENT

enum return_value_def
{
#define DEFINE_ZFS_RET(NUMBER, NAME, DESCRIPTION) \
	NAME = NUMBER,
#include "zfs-ret.def"
	ZFS_LAST_AND_UNUSED
};
#undef DEFINE_ZFS_RET

static const value_string packetreturnnames[] = {
#define DEFINE_ZFS_RET(NUMBER, NAME, DESCRIPTION) \
	{NAME, STR(NAME)},
#include "zfs-ret.def"
	{0, NULL}
};
#undef DEFINE_ZFS_RET

typedef enum connection_speed_def
{
  CONNECTION_SPEED_NONE = 0,
  CONNECTION_SPEED_SLOW,
  CONNECTION_SPEED_FAST,
  CONNECTION_SPEED_LAST_AND_UNUSED
} connection_speed;

static const value_string packetspeednames[] = {
	{CONNECTION_SPEED_NONE, "CONNECTION_SPEED_NONE"},
	{CONNECTION_SPEED_SLOW, "CONNECTION_SPEED_SLOW"},
	{CONNECTION_SPEED_FAST, "CONNECTION_SPEED_FAST"},
	{CONNECTION_SPEED_LAST_AND_UNUSED, "CONNECTION_SPEED_LAST_AND_UNUSED"},
	{0, NULL}
};


/* IF PROTO exposes code to other dissectors, then it must be exported
   in a header file. If not, a header file is not needed at all. */
//#include "packet-zfsd.h"

/* Forward declaration we need below (if using proto_reg_handoff...
   as a prefs callback)       */
void proto_reg_handoff_zfsd(void);

/* Initialize the protocol and registered fields */
static int proto_zfsd = -1;
static int hf_zfsd_length = -1;
static int hf_zfsd_type = -1;
static int hf_zfsd_request_id = -1;
static int hf_zfsd_response_id = -1;
static int hf_zfsd_function = -1;
static int hf_zfsd_status = -1;

static int hf_zfs_path = -1;

static int hf_args = -1;
static int hf_void = -1;

/* zfs_fh*/
static int hf_zfsd_fh = -1;
static int hf_zfsd_fh_sid = -1;
static int hf_zfsd_fh_vid = -1;
static int hf_zfsd_fh_dev = -1;
static int hf_zfsd_fh_ino = -1;
static int hf_zfsd_fh_gen = -1;

static int hf_zfsd_node_name = -1;

static int hf_zfsd_vid = -1;

static int hf_zfsd_open_flags = -1;

#define MD5_SIZE 16
#define ZFS_VERIFY_LEN MD5_SIZE

static int hf_zfsd_cap_flags = -1;
static int hf_zfsd_cap_verify = -1;

static int hf_zfsd_readdir_cookie = -1;
static int hf_zfsd_readdir_count = -1;

//TODO: add strings for transormation
static int hf_zfsd_connection_speed = -1;

static int hf_zfsd_dir_name = -1;

static int hf_zfsd_md5_count = -1;
static int hf_zfsd_md5_ignore_changes = -1;
static int hf_zfsd_md5_offset = -1;
static int hf_zfsd_md5_length = -1;

static int hf_zfsd_read_offset = -1;
static int hf_zfsd_read_count = -1;

/* type fattr */
static int hf_fattr = -1;
static int hf_fattr_dev = -1;
static int hf_fattr_ino = -1;
static int hf_fattr_version = -1;
static int hf_fattr_type = -1;
static int hf_fattr_mode = -1;
static int hf_fattr_nlink = -1;
static int hf_fattr_uid = -1;
static int hf_fattr_gid = -1;
static int hf_fattr_rdev = -1;
static int hf_fattr_size = -1;
static int hf_fattr_blocks = -1;
static int hf_fattr_blksize = -1;
static int hf_fattr_atime = -1;
static int hf_fattr_mtime = -1;
static int hf_fattr_ctime = -1;

/* type sattr */
static int hf_sattr = -1;
static int hf_sattr_mode = -1;
static int hf_sattr_uid = -1;
static int hf_sattr_gid = -1;
static int hf_sattr_size = -1;
static int hf_sattr_atime = -1;
static int hf_sattr_mtime = -1;

/* md5sum_res */
static int hf_md5sum_res_count = -1;
static int hf_md5sum_res_size = -1;
static int hf_md5sum_res_version = -1;

static int hf_md5sum_res_offset = -1;
static int hf_md5sum_res_length = -1;
static int hf_md5sum_res_sum = -1;

/* data_buffer */
static int hf_data_buffer = -1;
static int hf_data_buffer_size = -1;
static int hf_data_buffer_content = -1;

/* read_res */
static int hf_read_res_version = -1;

/* dir_list */
static int hf_dir_list_count = -1;
static int hf_dir_list_eof = -1;

/* dir_entry */
static int hf_dir_entry = -1;
static int hf_dir_entry_ino = -1;
static int hf_dir_entry_cookie = -1;
static int hf_dir_entry_filename = -1;

/* write_res */
static int hf_write_res_written = -1;
static int hf_write_res_version = -1;

/* create args */
static int hf_create_args_flags = -1;

/* mknod_args */
static int hf_mknod_args_type = -1;
static int hf_mknod_args_rdev = -1;

/* write_args */
static int hf_write_args_offset = -1;

/* reintegrate_args */
static int hf_reintegrate_args_status = -1;

/* reintegrate_add_args */
static int hf_reintegrate_add_args_filename = -1;

/* reintegrate_del_args */
static int hf_reintegrate_del_args_filename = -1;
static int hf_reintegrate_del_args_status = -1;

/* reintegrate_ver_args */
static int hf_reintegrate_ver_args_version_inc = -1;

/* Global sample port pref */
static guint gPORT_PREF = 12343;

/* Initialize the subtree pointers */
static gint ett_zfsd = -1;
static gint ett_args = -1;
static gint ett_type_zfs_fh = -1;
static gint ett_type_fattr = -1;
static gint ett_type_sattr = -1;
static gint ett_type_data_buffer = -1;
static gint ett_type_dir_entry = -1;

#define FRAME_HEADER_LEN 8

typedef struct {
	guint32 request_id;
	guint32 request_func;
} zfsd_entry_t;

static GMemChunk *zfsd_entries = NULL;

static guint
get_zfsd_message_len(packet_info * UNUSED(pinfo), tvbuff_t *tvb, int offset)
{
	return (guint)tvb_get_letohl(tvb, offset); /* e.g. length is at offset 0 */
}

static int 
dissect_zfsd_message_arg_zfs_fh(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	//proto_item_add_subtree
	proto_item * ti = NULL;
	proto_tree * zfs_fh_tree = NULL;
	offset = ALIGN_4(offset);
	ti = proto_tree_add_item(tree, hf_zfsd_fh, tvb, offset, 20, FALSE);
	zfs_fh_tree = proto_item_add_subtree(ti, ett_type_zfs_fh);
	proto_tree_add_item(zfs_fh_tree, hf_zfsd_fh_sid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(zfs_fh_tree, hf_zfsd_fh_vid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(zfs_fh_tree, hf_zfsd_fh_dev, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(zfs_fh_tree, hf_zfsd_fh_ino, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(zfs_fh_tree, hf_zfsd_fh_gen, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}

static int
dissect_zfsd_message_type_str(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, const int hfindex, int offset)
{
	guint32 str_len = 0;
	offset = ALIGN_4(offset);
	str_len = tvb_get_letohl(tvb, offset); offset += 4;
	/* add terminating zero */
	str_len += 1;
	proto_tree_add_item(tree, hfindex, tvb, offset, str_len, FALSE); offset += str_len;
	return offset;
}


static int
dissect_zfsd_message_arg_stage1_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_zfsd_node_name, offset);
}

static int
dissect_zfsd_message_arg_stage1_args_res(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_zfsd_node_name, offset);
}

static int
dissect_zfsd_message_arg_stage2_args(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	proto_tree_add_item(tree, hf_zfsd_connection_speed, tvb, offset, 1, FALSE);offset += 1;
	return offset;
}

static int
dissect_zfsd_message_arg_volume_root_args(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_zfsd_connection_speed, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}


static int
dissect_zfsd_message_arg_open_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_zfsd_open_flags, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}

static int
dissect_zfsd_message_arg_zfs_cap(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_zfsd_cap_flags, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(tree, hf_zfsd_cap_verify, tvb, offset, ZFS_VERIFY_LEN, FALSE); offset += ZFS_VERIFY_LEN;
	return offset;
}

static int
dissect_zfsd_message_arg_read_dir_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_cap(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_zfsd_readdir_cookie, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(tree, hf_zfsd_readdir_count, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}

static int
dissect_zfsd_message_arg_dir_op_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_zfsd_dir_name, offset);
}

static int
dissect_zfsd_message_arg_md5sum_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	int offset_offset;
	int length_offset;
	guint32 i;
	guint32 count;

	offset = dissect_zfsd_message_arg_zfs_cap(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	count = tvb_get_letohl(tvb, offset);
	proto_tree_add_item(tree, hf_zfsd_md5_count, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(tree, hf_zfsd_md5_ignore_changes, tvb, offset, 1, FALSE); offset += 1;

	offset = ALIGN_8(offset);
	offset_offset = offset;

	length_offset = offset_offset + 8 * count;
	length_offset = ALIGN_4(length_offset);

	for (i = 0; i < count; ++i)
	{
		proto_tree_add_item(tree, hf_zfsd_md5_offset, tvb, offset_offset, 8, ENC_LITTLE_ENDIAN); offset_offset += 8;
		proto_tree_add_item(tree, hf_zfsd_length, tvb, length_offset, 4, ENC_LITTLE_ENDIAN); length_offset += 4;
	}

	return offset;
}

static int
dissect_zfsd_message_type_zfs_time(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, const int hfindex, int offset)
{
	offset = ALIGN_4(offset);
	/*TODO: decode zfs time*/
	proto_tree_add_item(tree, hfindex, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}

static int
dissect_zfsd_message_type_ftype(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, const int hfindex, int offset)
{
	proto_tree_add_item(tree, hfindex, tvb, offset, 1, FALSE); offset += 1;
	/* return encode_uchar (dc, (uchar) type); */
	return offset;
}

static int
dissect_zfsd_message_type_fattr(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	proto_item * ti = NULL;
	proto_tree * fattr_tree = NULL;

	offset = ALIGN_4(offset);
	ti = proto_tree_add_item(tree, hf_fattr, tvb, offset, 20 + 72, FALSE);
	fattr_tree = proto_item_add_subtree(ti, ett_type_fattr);

	proto_tree_add_item(fattr_tree, hf_fattr_dev, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(fattr_tree, hf_fattr_ino, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	offset = ALIGN_8(offset);
	proto_tree_add_item(fattr_tree, hf_fattr_version, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	offset = dissect_zfsd_message_type_ftype(tvb, pinfo, fattr_tree, hf_fattr_type, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(fattr_tree, hf_fattr_mode, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(fattr_tree, hf_fattr_nlink, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(fattr_tree, hf_fattr_uid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(fattr_tree, hf_fattr_gid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(fattr_tree, hf_fattr_rdev, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	offset = ALIGN_8(offset);
	proto_tree_add_item(fattr_tree, hf_fattr_size, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	proto_tree_add_item(fattr_tree, hf_fattr_blocks, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	offset = ALIGN_4(offset);
	proto_tree_add_item(fattr_tree, hf_fattr_blksize, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;

	offset = dissect_zfsd_message_type_zfs_time(tvb, pinfo, fattr_tree, hf_fattr_atime, offset);
	offset = dissect_zfsd_message_type_zfs_time(tvb, pinfo, fattr_tree, hf_fattr_mtime, offset);
	offset = dissect_zfsd_message_type_zfs_time(tvb, pinfo, fattr_tree, hf_fattr_ctime, offset);
	return offset;
}

static int
dissect_zfsd_message_type_sattr(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	proto_item * ti = NULL;
	proto_tree * sattr_tree = NULL;

	offset = ALIGN_4(offset);
	ti = proto_tree_add_item(tree, hf_sattr, tvb, offset, 32, FALSE);
	sattr_tree = proto_item_add_subtree(ti, ett_type_sattr);

	proto_tree_add_item(sattr_tree, hf_sattr_mode, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(sattr_tree, hf_sattr_uid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(sattr_tree, hf_sattr_gid, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	offset = ALIGN_8(offset);
	proto_tree_add_item(sattr_tree, hf_sattr_size, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	offset = ALIGN_4(offset);
	offset = dissect_zfsd_message_type_zfs_time(tvb, pinfo, sattr_tree, hf_sattr_atime, offset);
	offset = dissect_zfsd_message_type_zfs_time(tvb, pinfo, sattr_tree, hf_sattr_mtime, offset);
	return offset;
}

static int
dissect_zfsd_message_arg_dir_op_res(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_fattr(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_type_dir_entry(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{

	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_dir_entry_ino, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(tree, hf_dir_entry_cookie, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_dir_entry_filename, offset);
}

static int
dissect_zfsd_message_arg_dir_list(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	guint32 i;
	guint32 count;
	int size;
	proto_item * ti = NULL;
	proto_tree * dir_entry_tree = NULL;

	offset = ALIGN_4(offset);
	count = tvb_get_letohl(tvb, offset);
	proto_tree_add_item(tree, hf_dir_list_count, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	proto_tree_add_item(tree, hf_dir_list_eof, tvb, offset, 1, FALSE); offset += 1;

	for (i = 0; i < count; ++i)
	{
		size = dissect_zfsd_message_type_dir_entry(tvb, pinfo, NULL, offset);
		ti = proto_tree_add_item(tree, hf_dir_entry, tvb, offset, size - offset, FALSE);
		dir_entry_tree = proto_item_add_subtree(ti, ett_type_dir_entry);
		
		offset = dissect_zfsd_message_type_dir_entry(tvb, pinfo, dir_entry_tree, offset);
	}

	return offset;
}

static int
dissect_zfsd_message_arg_md5sum_res(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	int offset_offset;
	int length_offset;
	int md5_offset;
	guint32 i;
	guint32 count;

	offset = ALIGN_4(offset);
	count = tvb_get_letohl(tvb, offset);
	proto_tree_add_item(tree, hf_md5sum_res_count, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	offset = ALIGN_8(offset);
	proto_tree_add_item(tree, hf_md5sum_res_size, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	proto_tree_add_item(tree, hf_md5sum_res_version, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;

	offset = ALIGN_8(offset);
	offset_offset = offset;

	length_offset = offset_offset + 8 * count;
	length_offset = ALIGN_4(length_offset);

	md5_offset = length_offset + 4 * count;

	for (i = 0; i<count; ++i)
	{
		proto_tree_add_item(tree, hf_md5sum_res_offset, tvb, offset_offset, 8, ENC_LITTLE_ENDIAN); offset_offset += 8;
		proto_tree_add_item(tree, hf_md5sum_res_length, tvb, length_offset, 4, ENC_LITTLE_ENDIAN); length_offset += 4;
		proto_tree_add_item(tree, hf_md5sum_res_sum, tvb, md5_offset, ZFS_VERIFY_LEN, FALSE); md5_offset += MD5_SIZE;
	}

	return offset;
}

static int
dissect_zfsd_message_arg_read_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_cap(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_zfsd_read_offset, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	proto_tree_add_item(tree, hf_zfsd_read_count, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}


static int
dissect_zfsd_message_arg_void(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	proto_tree_add_item(tree, hf_void, tvb, offset, 0, FALSE);
	return offset;
}

static int
dissect_zfsd_message_arg_data_bufer(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	guint32 size;
	proto_item * ti = NULL;
	proto_tree * data_buffer_tree = NULL;
	offset = ALIGN_4(offset);
	size = tvb_get_letohl(tvb, offset);
	ti = proto_tree_add_item(tree, hf_data_buffer, tvb, offset, size + 4, FALSE);
	data_buffer_tree = proto_item_add_subtree(ti, ett_type_data_buffer);

	proto_tree_add_item(data_buffer_tree, hf_data_buffer_size, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	if (size > 0)
		proto_tree_add_item(data_buffer_tree, hf_data_buffer_content, tvb, offset, size, FALSE); offset += size;
	return offset;
}


static int
dissect_zfsd_message_arg_read_res(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_data_bufer(tvb, pinfo, tree, offset);

	offset = ALIGN_8(offset);
	proto_tree_add_item(tree, hf_read_res_version, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;

	return offset;
}

static int
dissect_zfsd_message_arg_write_res(tvbuff_t *tvb, packet_info *UNUSED(pinfo), proto_tree *tree, int offset)
{
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_write_res_written, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	offset = ALIGN_8(offset);
	proto_tree_add_item(tree, hf_write_res_version, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	return offset;
}

static int
dissect_zfsd_message_arg_zfs_path(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_zfs_path, offset);
}

static int
dissect_zfsd_message_arg_read_link_res(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_arg_zfs_path(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_setattr_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_sattr(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_create_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_create_args_flags, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return dissect_zfsd_message_type_sattr(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_mkdir_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_sattr(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_rename_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_link_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_symlink_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
	offset = dissect_zfsd_message_arg_zfs_path(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_sattr(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_mknod_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_dir_op_args(tvb, pinfo, tree, offset);
	offset =  dissect_zfsd_message_type_sattr(tvb, pinfo, tree, offset);
	offset = dissect_zfsd_message_type_ftype(tvb, pinfo, tree, hf_mknod_args_type, offset);
	offset = ALIGN_4(offset);
	proto_tree_add_item(tree, hf_mknod_args_rdev, tvb, offset, 4, ENC_LITTLE_ENDIAN); offset += 4;
	return offset;
}

static int
dissect_zfsd_message_arg_reread_config_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_arg_zfs_path(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_write_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_cap(tvb, pinfo, tree, offset);
	offset = ALIGN_8(offset);
	proto_tree_add_item(tree, hf_write_args_offset, tvb, offset, 8, ENC_LITTLE_ENDIAN); offset += 8;
	return dissect_zfsd_message_arg_data_bufer(tvb, pinfo, tree, offset);
}

static int
dissect_zfsd_message_arg_reintegrate_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	proto_tree_add_item(tree, hf_reintegrate_args_status, tvb, offset, 1, FALSE); offset += 1;
	return offset;
}

static int
dissect_zfsd_message_arg_reintegrate_add_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	return dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_reintegrate_add_args_filename, offset);
}

static int
dissect_zfsd_message_arg_reintegrate_del_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = dissect_zfsd_message_type_str(tvb, pinfo, tree, hf_reintegrate_del_args_filename, offset);
	proto_tree_add_item(tree, hf_reintegrate_del_args_status, tvb, offset, 1, FALSE); offset += 1;
	return offset;
}

static int
dissect_zfsd_message_arg_reintegrate_ver_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	offset = dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
	offset = ALIGN_8(offset);
	proto_tree_add_item(tree, hf_reintegrate_ver_args_version_inc, tvb, offset, 1, FALSE); offset += 1;
	return offset;
}

static int
dissect_zfsd_message_arg_invalidate_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
	return dissect_zfsd_message_arg_zfs_fh(tvb, pinfo, tree, offset);
}

struct args_id_to_function_mapping
{
	guint32 function_id;
	int ( * dissect_request_arg)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset);
	int ( * dissect_response_arg)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset);
};

struct args_id_to_function_mapping  arg_sid_to_function[] = {
	{0,  NULL, NULL}, /*NULL*/
	{1,  dissect_zfsd_message_arg_data_bufer, dissect_zfsd_message_arg_data_bufer}, /*PING*/
	{2,  dissect_zfsd_message_arg_void, dissect_zfsd_message_arg_zfs_fh}, /*ROOT*/
	{3,  dissect_zfsd_message_arg_volume_root_args, dissect_zfsd_message_arg_dir_op_res}, /*VOLUME_ROOT*/
	{4,  dissect_zfsd_message_arg_zfs_fh, dissect_zfsd_message_type_fattr}, /*GETATTR*/
	{5,  dissect_zfsd_message_arg_setattr_args, dissect_zfsd_message_type_fattr}, /*SETATTR*/ 
	{6,  dissect_zfsd_message_arg_dir_op_args, dissect_zfsd_message_arg_dir_op_res}, /*LOOKUP*/
	{7,  dissect_zfsd_message_arg_create_args, dissect_zfsd_message_arg_void}, /*CREATE*/
	{8,  dissect_zfsd_message_arg_open_args, dissect_zfsd_message_arg_zfs_cap}, /*OPEN*/
	{9,  dissect_zfsd_message_arg_zfs_cap, dissect_zfsd_message_arg_void}, /*CLOSE*/
	{10, dissect_zfsd_message_arg_read_dir_args, dissect_zfsd_message_arg_dir_list}, /*READDIR*/
	{11, dissect_zfsd_message_arg_mkdir_args, dissect_zfsd_message_arg_dir_op_res}, /*MKDIR*/
	{12, dissect_zfsd_message_arg_dir_op_args, dissect_zfsd_message_arg_void}, /*RMDIR*/
	{13, dissect_zfsd_message_arg_rename_args, dissect_zfsd_message_arg_void}, /*RENAME*/
	{14, dissect_zfsd_message_arg_link_args, dissect_zfsd_message_arg_void}, /*LINK*/
	{15, dissect_zfsd_message_arg_dir_op_args, dissect_zfsd_message_arg_void}, /*UNLINK*/
	{16, dissect_zfsd_message_arg_read_args, dissect_zfsd_message_arg_read_res}, /*READ*/ 
	{17, dissect_zfsd_message_arg_write_args, dissect_zfsd_message_arg_write_res}, /*WRITE*/
	{18, dissect_zfsd_message_arg_zfs_fh, dissect_zfsd_message_arg_read_link_res}, /*READLINK*/
	{19, dissect_zfsd_message_arg_symlink_args, dissect_zfsd_message_arg_dir_op_res}, /*SYMLINK*/
	{20, dissect_zfsd_message_arg_mknod_args, dissect_zfsd_message_arg_dir_op_res}, /*MKNOD*/
	{21, dissect_zfsd_message_arg_stage1_args, dissect_zfsd_message_arg_stage1_args_res}, /*AUTH_STAGE1*/
	{22, dissect_zfsd_message_arg_stage2_args, dissect_zfsd_message_arg_void}, /*AUTH_STAGE2*/
	{23, dissect_zfsd_message_arg_md5sum_args, dissect_zfsd_message_arg_md5sum_res}, /*MD5SUM*/
	{24, dissect_zfsd_message_arg_zfs_fh, dissect_zfsd_message_arg_zfs_path}, /*FILE_INFO*/
	{25, dissect_zfsd_message_arg_reread_config_args, NULL}, /*REREAD_CONFIG, DIR_ONEWAY*/
	{26, dissect_zfsd_message_arg_reintegrate_args, dissect_zfsd_message_arg_void}, /*REINTEGRATE*/
	{27, dissect_zfsd_message_arg_reintegrate_add_args, dissect_zfsd_message_arg_void}, /*REINTEGRATE_ADD*/
	{28, dissect_zfsd_message_arg_reintegrate_del_args, dissect_zfsd_message_arg_void}, /*REINTEGRATE_DEL*/
	{29, dissect_zfsd_message_arg_reintegrate_ver_args, dissect_zfsd_message_arg_void}, /*REINTEGRATE_SET*/
	{30, dissect_zfsd_message_arg_invalidate_args, dissect_zfsd_message_arg_void} /*INVALIDATE*/
};

/* disect request and oneway args */
static void
dissect_zfsd_request_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, guint32 function_id)
{
	size_t i;
	int size = offset;
	proto_item *ti = NULL;

	proto_tree * response_tree = NULL;
	if (tree == NULL)
		return;

	for (i = 0; i < sizeof(arg_sid_to_function) / sizeof(struct args_id_to_function_mapping); ++i)
	{
		if (arg_sid_to_function[i].function_id == function_id)
		{
			if (arg_sid_to_function[i].dissect_request_arg != NULL)
			{
				/* add Args: subtree*/
				size = arg_sid_to_function[i].dissect_request_arg(tvb, pinfo, NULL, offset);
				ti = proto_tree_add_item(tree, hf_args, tvb, offset, size - offset, FALSE);
				response_tree = proto_item_add_subtree(ti, ett_args);
				arg_sid_to_function[i].dissect_request_arg(tvb, pinfo, response_tree, offset);
			}

			return;
		}
	}
}

static void
dissect_zfsd_response_args(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, guint32 function_id)
{
	size_t i;
	int size = offset;
	proto_item *ti = NULL;

	proto_tree * response_tree = NULL;
	if (tree == NULL)
		return;

	for (i = 0; i < sizeof(arg_sid_to_function) / sizeof(struct args_id_to_function_mapping); ++i)
	{
		if (arg_sid_to_function[i].function_id == function_id)
		{
			if (arg_sid_to_function[i].dissect_response_arg != NULL)
			{
				/* add Args: subtree*/
				size = arg_sid_to_function[i].dissect_response_arg(tvb, pinfo, NULL, offset);
				ti = proto_tree_add_item(tree, hf_args, tvb, offset, size - offset, FALSE);
				response_tree = proto_item_add_subtree(ti, ett_args);
				arg_sid_to_function[i].dissect_response_arg(tvb, pinfo, response_tree, offset);
			}

			return;
		}
	}
}



static void
dissect_zfsd_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{

	conversation_t * conv;
	zfsd_entry_t * zfsd_entry_ptr;
	guint32 zfs_message_length;
	guint8 zfs_message_dir;
	guint32 zfs_request_id;
	guint32 zfsd_request_func =  ZFS_PROC_LAST_AND_UNUSED;
	gint offset = 0;
	guint32 zfs_message_status = ZFS_OK;

	/* packat has at least minimal length */
	DISSECTOR_ASSERT(tvb_length(tvb) >= ZFS_MESSAGE_LEN_MIN);

	/* gets one uint32 which is little endian sized */
	zfs_message_length = tvb_get_letohl(tvb, offset);
	offset += sizeof(zfs_message_length);

	/* wrong message len */
	DISSECTOR_ASSERT(tvb_length(tvb) == zfs_message_length);

	/* Make entries in Protocol column and Info column on summary display */
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "zfsd");
	/* gets message direction */
	zfs_message_dir = tvb_get_guint8(tvb, offset); offset +=1;
	offset = ALIGN_4(offset);
	zfs_request_id = tvb_get_letohl(tvb, offset); offset +=4;


	switch (zfs_message_dir)
	{
		case DIR_REQUEST:
			conv = conversation_new(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype,
			            pinfo->srcport, pinfo->destport, 0);
			if (conv != NULL)
			{
				zfsd_entry_ptr = g_mem_chunk_alloc(zfsd_entries);
				zfsd_entry_ptr->request_id = zfs_request_id;
				zfsd_request_func = tvb_get_letohl(tvb, offset); offset +=4;
				zfsd_entry_ptr->request_func = zfsd_request_func;
				conversation_add_proto_data(conv, proto_zfsd, (void *)zfsd_entry_ptr);

			}
			break;
		case DIR_REPLY:
			conv = find_conversation(pinfo->fd->num, &pinfo->src, &pinfo->dst,
			        pinfo->ptype, pinfo->srcport, pinfo->destport, 0);
			if (conv != NULL)
			{
				zfsd_entry_ptr = (zfsd_entry_t*)conversation_get_proto_data(conv, proto_zfsd);
				if (zfsd_entry_ptr != NULL && zfsd_entry_ptr->request_id == zfs_request_id)
					zfsd_request_func = zfsd_entry_ptr->request_func;
			}
			break;
	}

	col_add_fstr(pinfo->cinfo, COL_INFO, "Func: %s, Dir: %s, Id: 0x%08x, Len: %u",
		val_to_str(zfsd_request_func, packetfunctionnames, "unknown (0x%02x)"),
		val_to_str(zfs_message_dir, packettypenames, "Unknown (0x%02x)"),
		zfs_request_id, zfs_message_length);

	if (tree != NULL)
	{
		proto_item *ti = NULL;
		proto_tree *zfsd_tree = NULL;
		gint packet_offset = 0;
		/* from begin 0 to end -1 */
		ti = proto_tree_add_item(tree, proto_zfsd, tvb, 0, -1, FALSE);
		zfsd_tree = proto_item_add_subtree(ti, ett_zfsd);
		/* gets packet length */
		packet_offset = ALIGN_4(packet_offset);
		proto_tree_add_item(zfsd_tree, hf_zfsd_length, tvb, packet_offset, 4, ENC_LITTLE_ENDIAN); packet_offset += 4;
		/* gets packet type (direction) */
		packet_offset = ALIGN_1(packet_offset);
		proto_tree_add_item(zfsd_tree, hf_zfsd_type, tvb, packet_offset, 1, ENC_LITTLE_ENDIAN); packet_offset += 1;

		switch (zfs_message_dir)
		{
			case DIR_ONEWAY:
				// gets oneway request function
			case DIR_REQUEST:
				/* gets reuqest id */
				packet_offset = ALIGN_4(packet_offset);
				proto_tree_add_item(zfsd_tree, hf_zfsd_request_id, tvb, packet_offset, 4, ENC_LITTLE_ENDIAN); packet_offset += 4;

				/* gets request function */
				packet_offset = ALIGN_4(packet_offset);
				proto_tree_add_item(zfsd_tree, hf_zfsd_function, tvb, packet_offset, 4, ENC_LITTLE_ENDIAN); packet_offset += 4;

				/* dissects request args */
				dissect_zfsd_request_args(tvb, pinfo, zfsd_tree, packet_offset, tvb_get_letohl(tvb, packet_offset - 4));
				break;
			case DIR_REPLY:
				/* gets response id*/
				packet_offset = ALIGN_4(packet_offset);
				proto_tree_add_item(zfsd_tree, hf_zfsd_response_id, tvb, packet_offset, 4, ENC_LITTLE_ENDIAN); packet_offset += 4;
				proto_tree_add_text(zfsd_tree, tvb, packet_offset - 4, 4, "Function: %s (0x%08x)",
					val_to_str(zfsd_request_func, packetfunctionnames, "unknown (0x%02x)"), zfsd_request_func);
				
				/* gets reply return vaule (status) */
				packet_offset = ALIGN_4(packet_offset);
				zfs_message_status = tvb_get_letohl(tvb, offset);
				proto_tree_add_item(zfsd_tree, hf_zfsd_status, tvb, packet_offset, 4, ENC_LITTLE_ENDIAN); packet_offset += 4;

				/* gets response args */
				if (zfs_message_status == ZFS_OK)
					dissect_zfsd_response_args(tvb, pinfo, zfsd_tree, packet_offset, zfsd_request_func);
				break;
		}
	}
}

/* Code to actually dissect the packets */
static int
dissect_zfsd(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	tcp_dissect_pdus(tvb, pinfo, tree, TRUE, FRAME_HEADER_LEN,
	                         get_zfsd_message_len, dissect_zfsd_message);
	return tvb_length(tvb);
}

#define zfsd_init_count 20
static void
zfsd_dissector_init()
{
	/* destroy memory chunks if needed */
	if (zfsd_entries)
		g_mem_chunk_destroy(zfsd_entries);

	/* now create memory chunks */
	zfsd_entries = g_mem_chunk_new("zfsd_proto_entries",
			sizeof(zfsd_entry_t),
			zfsd_init_count * sizeof(zfsd_entry_t),
			G_ALLOC_AND_FREE);

}

/* Register the protocol with Wireshark */

/* this format is require because a script is used to build the C function
   that calls all the protocol registration.
*/

void
proto_register_zfsd(void)
{
	module_t *zfsd_module;

/* Setup list of header fields  See Section 1.6.1 for details */
	static hf_register_info hf[] = {
		{ &hf_args,
			{
				"Args", "zfsd.args",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_void,
			{
				"void", "zfsd.types.void",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_length,
			{
				"Length", "zfsd.length",
				FT_UINT32, BASE_DEC,
				NULL, 0,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_type,
			{
				"Type", "zfsd.type",
				FT_UINT8, BASE_DEC,
				VALS(packettypenames), 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_request_id,
			{
				"Request Id", "zfsd.id",
				FT_UINT32, BASE_HEX,
				NULL, 0,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_response_id,
			{
				"Response Id", "zfsd.id",
				FT_UINT32, BASE_HEX,
				NULL, 0,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_function,
			{
				"Function", "zfsd.function",
				FT_UINT32, BASE_HEX,
				VALS(packetfunctionnames), 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_status,
			{
				"Status", "zfsd.status",
				FT_INT32, BASE_DEC,
				VALS(packetreturnnames), 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_fh,
			{
				"zfs_fh", "zfsd.types.zfs_fh",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}

		},
		{ &hf_zfsd_fh_sid,
			{
				"zfs_fh.sid", "zfsd.types.zfs_fh.sid",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_fh_vid,
			{
				"zfs_fh.vid", "zfsd.types.zfs_fh.vid",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_fh_dev,
			{
				"zfs_fh.dev", "zfsd.types.zfs_fh.dev",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_fh_ino,
			{
				"zfs_fh.ino", "zfsd.types.zfs_fh.ino",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_fh_gen,
			{
				"zfs_fh.gen", "zfsd.types.zfs_fh.gen",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_node_name,
			{
				"node_name", "zfsd.node_name",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_connection_speed,
			{
				"connection_speed", "zfsd.connection_speed",
				FT_UINT8, BASE_DEC,
				VALS(packetspeednames), 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_vid,
			{
				"volume id", "zfsd.vid",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_open_flags,
			{
				"open flags", "zfsd.open_flags",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_cap_flags,
			{
				"cap flags", "zfsd.cap_flags",
				FT_UINT32, BASE_HEX,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_cap_verify,
			{
				"cap verify", "zfsd.cap_verify",
				FT_BYTES, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_readdir_cookie,
			{
				"readdir cookie", "zfsd.readdir_cookie",
				FT_INT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_readdir_count,
			{
				"readdir count", "zfsd.readdir_count",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_dir_name,
			{
				"dir_name", "zfsd.dir_name",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_md5_count,
			{
				"md5 count", "zfsd.md5_count",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_md5_ignore_changes,
			{
				"md5 ignore changes", "zfsd.md5_ignore_changes",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_md5_offset,
			{
				"md5 offset", "zfsd.offset",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_md5_length,
			{
				"md5 length", "zfsd.md5_length",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_read_offset,
			{
				"read offset", "zfsd.read_offset",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfsd_read_count,
			{
				"read count", "zfsd.read_count",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr,
			{
				"sattr", "zfsd.types.sattr",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_mode,
			{
				"sattr.mode", "zfsd.sattr.mode",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_uid,
			{
				"sattr.uid", "zfsd.sattr.uid",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_gid,
			{
				"sattr.gid", "zfsd.sattr.gid",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_size,
			{
				"sattr.size", "zfsd.sattr.size",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_atime,
			{
				"sattr.atime", "zfsd.sattr.atime",
				FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_sattr_mtime,
			{
				"sattr.mtime", "zfsd.sattr.mtime",
				FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
				NULL, 0x00,
				NULL, HFILL
			}
		},

		{ &hf_fattr,
			{
				"fattr", "zfsd.types.fattr",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_dev,
			{
				"fattr.dev", "zfsd.fattr.dev",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_ino,
			{
				"fattr.ino", "zfsd.fattr.ino",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_version,
			{
				"fattr.version", "zfsd.fattr.version",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		/*TODO: fixmee type*/
		{ &hf_fattr_type,
			{
				"fattr.type", "zfsd.fattr.type",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_mode,
			{
				"fattr.mode", "zfsd.fattr.mode",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_nlink,
			{
				"fattr.nlink", "zfsd.fattr.nlink",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_uid,
			{
				"fattr.uid", "zfsd.fattr.uid",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_gid,
			{
				"fattr.gid", "zfsd.fattr.gid",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_rdev,
			{
				"fattr.rdev", "zfsd.fattr.rdev",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_size,
			{
				"fattr.size", "zfsd.fattr.size",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_blocks,
			{
				"fattr.blocks", "zfsd.fattr.blocks",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_blksize,
			{
				"fattr.blksize", "zfsd.fattr.blksize",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_atime,
			{
				"fattr.atime", "zfsd.fattr.atime",
				FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_mtime,
			{
				"fattr.mtime", "zfsd.fattr.mtime",
				FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_fattr_ctime,
			{
				"fattr.ctime", "zfsd.fattr.ctime",
				FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_count,
			{
				"md5sum_res.count", "zfsd.md5sum_res.count",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_size,
			{
				"md5sum_res.size", "zfsd.md5sum_res.size",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_version,
			{
				"md5sum_res.version", "zfsd.md5sum_res.version",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_offset,
			{
				"md5sum_res.offset", "zfsd.md5sum_res.offset",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_length,
			{
				"md5sum_res.length", "zfsd.md5sum_res.length",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_md5sum_res_sum,
			{
				"md5sum_res.sum", "zfsd.md5sum_res.sum",
				FT_BYTES, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_data_buffer,
			{
				"data_buffer", "zfsd.types.data_buffer",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},

		{ &hf_data_buffer_size,
			{
				"data_buffer.size", "zfsd.data_buffer.size",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_data_buffer_content,
			{
				"data_buffer.content", "zfsd.data_buffer.content",
				FT_BYTES, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_read_res_version,
			{
				"read_res.version", "zfsd.read_res.version",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_list_count,
			{
				"dir_list.count", "zfsd.dir_list.count",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_list_eof,
			{
				"dir_list.eof", "zfsd.dir_list.eof",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_entry,
			{
				"dir_entry", "zfsd.types.dir_entry",
				FT_NONE, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_entry_ino,
			{
				"dir_entry.ino", "zfsd.types.dir_entry.ino",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_entry_cookie,
			{
				"dir_entry.cookie", "zfsd.types.dir_entry.cookie",
				FT_INT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_dir_entry_filename,
			{
				"dir_entry.filename", "zfsd.types.dir_entry.filename",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_write_res_written,
			{
				"write_res.written", "zfsd.types.write_res.written",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_write_res_version,
			{
				"write_res.version", "zfsd.types.write_res.version",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_zfs_path,
			{
				"zfs_path", "zfsd.types.zfs_path",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_create_args_flags,
			{
				"create_args.flags", "zfsd.types.create_args.flags",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_mknod_args_rdev,
			{
				"mknod_args.rdev", "zfsd.types.mknod_args.rdev",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_mknod_args_type,
			{
				"mknod_args.type", "zfsd.types.mknod_args.type",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_write_args_offset,
			{
				"write_args.offset", "zfsd.types.write_args.offset",
				FT_UINT32, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_reintegrate_args_status,
			{
				"reintegrate_args.status", "zfsd.types.reintegrate_args.status",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_reintegrate_add_args_filename,
			{
				"filename", "zfsd.types.reintegrate_add_args.filename",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_reintegrate_ver_args_version_inc,
			{
				"version_inc", "zfsd.types.reintegrate_ver_args_version.filename",
				FT_UINT64, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_reintegrate_del_args_status,
			{
				"reintegrate_del_args.status", "zfsd.types.reintegrate_del_args.status",
				FT_UINT8, BASE_DEC,
				NULL, 0x00,
				NULL, HFILL
			}
		},
		{ &hf_reintegrate_del_args_filename,
			{
				"reintegrate_del_args.filename", "zfsd.types.reintegrate_del_args.filename",
				FT_STRING, BASE_NONE,
				NULL, 0x00,
				NULL, HFILL
			}
		}
	};

/* Setup protocol subtree array */
	static gint *ett[] = {
		&ett_zfsd,
		&ett_args,
		&ett_type_zfs_fh,
		&ett_type_sattr,
		&ett_type_fattr,
		&ett_type_data_buffer,
		&ett_type_dir_entry
	};

/* Register the protocol name and description */
	proto_zfsd = proto_register_protocol(
		"Zlomek FS Communication Protocol", 	/* name 	*/
	    	"ZFSD", 				/* short name 	*/
		"zfsd" 					/* abbrev 	*/
		);

	register_init_routine(&zfsd_dissector_init);
	proto_register_field_array(proto_zfsd, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
	zfsd_module = prefs_register_protocol(proto_zfsd,
	    proto_reg_handoff_zfsd);
}

void
proto_reg_handoff_zfsd(void)
{
	static dissector_handle_t zfsd_handle;
	zfsd_handle = new_create_dissector_handle(dissect_zfsd, proto_zfsd);
	dissector_add("tcp.port", gPORT_PREF, zfsd_handle);
}

