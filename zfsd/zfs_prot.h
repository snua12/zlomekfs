/* ZFS protocol.
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

#ifndef ZFS_PROT_H
#define ZFS_PROT_H

#include "system.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ZFS_PORT 12323
#define ZFS_MAXDATA 8192
#define ZFS_MAXPATHLEN 1023
#define ZFS_MAXNAMELEN 255

/* Error codes.  */
#define ZFS_OK			0
#define ZFS_REQUEST_TOO_LONG	-1	/* Request was too long.  */
#define ZFS_INVALID_REQUEST	-2	/* Request was not well encoded. */
#define ZFS_UNKNOWN_FUNCTION	-3	/* Unknown function in request.  */
  
typedef enum direction_def
{
  DIR_REQUEST,
  DIR_REPLY,
  DIR_MAX_AND_UNUSED
} direction;

typedef struct data_buffer_def
{
  unsigned int len;
  char buf[ZFS_MAXDATA];
} data_buffer;

typedef struct string_def
{
  unsigned int len;
  char *buf;
} string;

typedef enum ftype_def
{
  FT_BAD,
  FT_REG,
  FT_DIR,
  FT_LNK,
  FT_BLK,
  FT_CHR,
  FT_SOCK,
  FT_FIFO,
  FT_MAX_AND_UNUSED
} ftype;

typedef struct zfs_fh_def
{
  unsigned int sid;
  unsigned int vid;
  unsigned int dev;
  unsigned int ino;
} zfs_fh;

typedef struct zfs_time_def
{
  unsigned int sec;
  unsigned int usec;
} zfs_time;

typedef struct fattr_def
{
  ftype type;
  unsigned int mode;
  unsigned int nlink;
  unsigned int uid;
  unsigned int gid;
  unsigned int rdev;
  uint64_t size;
  uint64_t blocks;
  unsigned int blksize;
  unsigned int generation;
  uint64_t fversion;
  unsigned int sid;
  unsigned int vid;
  unsigned int fsid;
  unsigned int fileid;
  zfs_time atime;
  zfs_time mtime;
  zfs_time ctime;
} fattr;

typedef struct sattr_def
{
  unsigned int mode;
  unsigned int uid;
  unsigned int gid;
  uint64_t size;
  zfs_time atime;
  zfs_time mtime;
} sattr;

typedef string filename;

typedef string zfs_path;

typedef struct sattr_args_def
{
  zfs_fh file;
  sattr attributes;
} sattr_args;

typedef struct dir_op_args_def
{
  zfs_fh dir;
  filename name;
} dir_op_args;

typedef struct dir_op_res_def
{
  zfs_fh file;
  fattr attributes;
} dir_op_res;

typedef struct open_name_args_def
{
  dir_op_args where;
  int flags;
  sattr attributes;
} open_name_args;

typedef struct open_res_def
{
  zfs_fh file;
} open_res;

typedef struct read_dir_args_def
{
  zfs_fh dir;
  int cookie;
  unsigned int count;
} read_dir_args;

typedef struct dir_entry_def
{
  zfs_fh fh;
  filename name;
  int cookie;
} dir_entry;

typedef struct dir_list_def
{
  unsigned int n;
  char eof;
  /* dir_entry dir_entries[];  */
} dir_list;

typedef struct read_dir_res_def
{
  dir_list reply;
} read_dir_res;

typedef struct rename_args_def
{
  dir_op_args from;
  dir_op_args to;
} rename_args;

typedef struct link_args_def
{
  zfs_fh from;
  dir_op_args to;
} link_args;

typedef struct read_args_def
{
  zfs_fh file;
  uint64_t offset;
  unsigned int count;
} read_args;

typedef struct read_res_def
{
  data_buffer data;
} read_res;

typedef struct write_args_def
{
  zfs_fh file;
  uint64_t offset;
  data_buffer data;
} write_args;

typedef struct write_res_def
{
  unsigned int written;
} write_res;

typedef struct read_link_res_def
{
  zfs_path path;
} read_link_res;

typedef struct symlink_args_def
{
  dir_op_args from;
  zfs_path to;
  sattr attributes;
} symlink_args;

typedef struct mknod_args_def
{
  dir_op_args where;
  sattr attributes;
  unsigned int rdev;
} mknod_args;

typedef union call_args_def
{
  unsigned int volume_root_args;
  zfs_fh getattr;
  sattr_args setattr;
  dir_op_args lookup;
  open_name_args open_by_name;
  zfs_fh open_by_fd;
  zfs_fh close;
  read_dir_args readdir;
  open_name_args mkdir;
  dir_op_args rmdir;
  rename_args rename;
  write_args write;
  zfs_fh readlink;
  symlink_args symlink;
  mknod_args mknod;
} call_args;

#ifdef __cplusplus
}
#endif

#endif
