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

#define ZFS_PORT 12323
#define ZFS_MAXDATA 8192
#define ZFS_MAXPATHLEN 1023
#define ZFS_MAXNAMELEN 255
#define ZFS_MAXNODELEN 256
#define ZFS_AUTH_LEN 16
#define ZFS_VERIFY_LEN 16

/* Error codes.
   System errors have positive numbers, ZFS errors have negative numbers.  */
#define ZFS_OK			0
#define ZFS_REQUEST_TOO_LONG	-1	/* Request was too long.  */
#define ZFS_INVALID_REQUEST	-2	/* Request was not well encoded. */
#define ZFS_UNKNOWN_FUNCTION	-3	/* Unknown function in request.  */
#define ZFS_INVALID_AUTH_LEVEL	-4	/* Remote node has not authenticated
					   enough yet.  */
#define ZFS_LAST_DECODED_ERROR	-99	/* Code of last error which is being
					   decoded from DC */

#define ZFS_INVALID_REPLY	-100	/* Reply was not well encoded. */
#define ZFS_ERROR_HAS_DC_REPLY	-150	/* Code of last error which has
					   a DC_REPLY.  */

#define ZFS_EXITING		-151	/* zfsd is exiting */
#define ZFS_COULD_NOT_CONNECT	-152	/* Could not connect to node.  */
#define ZFS_COULD_NOT_AUTH	-153	/* Could not authenticate with node.  */
#define ZFS_CONNECTION_CLOSED	-154	/* Connection closed while waiting for
					   reply.  */

typedef enum direction_def
{
  DIR_REQUEST,
  DIR_REPLY,
  DIR_LAST_AND_UNUSED
} direction;

typedef struct data_buffer_def
{
  unsigned int len;
  char buf[ZFS_MAXDATA];
} data_buffer;

typedef struct string_def
{
  unsigned int len;
  char *str;
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
  FT_LAST_AND_UNUSED
} ftype;

typedef struct zfs_fh_def
{
  unsigned int sid;		/* Server ID.  */
  unsigned int vid;		/* Volume ID.  */
  unsigned int dev;		/* Device ... */
  unsigned int ino;		/* ... and inode number of the file.  */
} zfs_fh;

typedef struct zfs_cap_def
{
  zfs_fh fh;
  unsigned int flags;
  unsigned char verify[ZFS_VERIFY_LEN];
} zfs_cap;

typedef unsigned int zfs_time;

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
  unsigned int dev;
  unsigned int ino;
  zfs_time atime;
  zfs_time mtime;
  zfs_time ctime;
} fattr;

#define SATTR_MODE	1
#define SATTR_CHOWN	2
#define SATTR_SIZE	4
#define SATTR_UTIME	8

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

typedef string nodename;

typedef struct volume_root_args_def
{
  unsigned int vid;
} volume_root_args;

typedef struct sattr_args_def
{
  zfs_fh file;
  sattr attr;
} sattr_args;

typedef struct dir_op_args_def
{
  zfs_fh dir;
  filename name;
} dir_op_args;

typedef struct dir_op_res_def
{
  zfs_fh file;
  fattr attr;
} dir_op_res;

typedef struct create_args_def
{
  dir_op_args where;
  unsigned int flags;
  sattr attr;
} create_args;

typedef struct create_res_def
{
  zfs_cap cap;
  zfs_fh file;
  fattr attr;
} create_res;

typedef struct open_args_def
{
  zfs_fh file;
  unsigned int flags;
} open_args;

typedef struct read_dir_args_def
{
  zfs_cap cap;
  int cookie;
  unsigned int count;
} read_dir_args;

typedef struct dir_entry_def
{
  unsigned int ino;
  int cookie;
  filename name;
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

typedef struct mkdir_args_def
{
  dir_op_args where;
  sattr attr;
} mkdir_args;

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
  zfs_cap cap;
  uint64_t offset;
  unsigned int count;
} read_args;

typedef struct write_args_def
{
  zfs_cap cap;
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
  sattr attr;
} symlink_args;

typedef struct mknod_args_def
{
  dir_op_args where;
  sattr attr;
  ftype type;
  unsigned int rdev;
} mknod_args;

typedef struct auth_stage1_args_def
{
  char auth[ZFS_AUTH_LEN];
  nodename node;
  /* int speed; */
} auth_stage1_args;

typedef struct auth_stage2_args_def
{
  char auth[ZFS_AUTH_LEN];
} auth_stage2_args;

typedef union call_args_def
{
  char null;
  char root;
  volume_root_args volume_root;
  zfs_fh getattr;
  sattr_args setattr;
  dir_op_args lookup;
  create_args create;
  open_args open;
  zfs_cap close;
  read_dir_args readdir;
  mkdir_args mkdir;
  dir_op_args rmdir;
  rename_args rename;
  link_args link;
  dir_op_args unlink;
  read_args read;
  write_args write;
  zfs_fh readlink;
  symlink_args symlink;
  mknod_args mknod;
  auth_stage1_args auth_stage1;
  auth_stage2_args auth_stage2;
} call_args;

/* Mapping file type -> file mode.  */
extern unsigned int ftype2mode[FT_LAST_AND_UNUSED];

/* Function numbers.  */
enum function_number_def
{
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
  ZFS_PROC_##NAME = NUMBER,
#include "zfs_prot.def"
  ZFS_PROC_LAST_AND_UNUSED
};
#undef DEFINE_ZFS_PROC

struct thread_def;
struct node_def;

/* Function headers.  */
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
  extern void zfs_proc_##FUNCTION##_server (ARGS *args,			\
					    struct thread_def *t);	\
  extern int zfs_proc_##FUNCTION##_client (struct thread_def *t,	\
					   ARGS *args,			\
					   struct node_def *nod);	\
  extern int zfs_proc_##FUNCTION##_client_1 (struct thread_def *t,	\
					     ARGS *args, int fd);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

extern char *zfs_strerror (int errnum);
extern void initialize_zfs_prot_c ();
extern void cleanup_zfs_prot_c ();

#endif
