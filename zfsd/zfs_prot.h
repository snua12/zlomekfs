/* ZFS protocol.
   Copyright (C) 2003, 2004 Josef Zlomek
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

#ifndef ZFS_PROT_H
#define ZFS_PROT_H

#include "system.h"

#ifdef __KERNEL__
# include <linux/types.h>
#else
# include <inttypes.h>
#endif

#include "md5.h"
#include "memory.h"

#define ZFS_PORT 12323
#define ZFS_MAXDATA 8192
#define ZFS_MAXPATHLEN 1023
#define ZFS_MAXNAMELEN 255
#define ZFS_MAXNODELEN 256
#define ZFS_VERIFY_LEN MD5_SIZE
#define ZFS_MAX_MD5_CHUNKS (ZFS_MAXDATA / (MD5_SIZE + 2 * sizeof (uint64_t)))
#define ZFS_MAX_DIR_ENTRIES (ZFS_MAXDATA / (4 * sizeof (uint32_t)))

/* Error codes.
   System errors have positive numbers, ZFS errors have negative numbers.  */
#define ZFS_OK			0
#define ZFS_REQUEST_TOO_LONG	-1	/* Request was too long.  */
#define ZFS_INVALID_REQUEST	-2	/* Request was not well encoded. */
#define ZFS_UNKNOWN_FUNCTION	-3	/* Unknown function in request.  */
#define ZFS_INVALID_AUTH_LEVEL	-4	/* Remote node has not authenticated
					   enough yet.  */
#define ZFS_INVALID_DIRECTION	-5	/* Different direction to direction
					   expected by function.  */
#define ZFS_STALE		-20	/* zfs_fh could not be found.  */
#define ZFS_BUSY		-21	/* file handle is being reintegrated */
#define ZFS_CHANGED		-22	/* The file has changed while doing
					   operation.  */
#define ZFS_METADATA_ERROR	-50	/* Error when accessing file containing
					   metadata.  */
#define ZFS_UPDATE_FAILED	-51	/* Error while updating a file.  */
#define ZFS_LAST_DECODED_ERROR	-99	/* Code of last error which is being
					   decoded from DC */

#define ZFS_REPLY_TOO_LONG	-100	/* Reply was too long.  */
#define ZFS_INVALID_REPLY	-101	/* Reply was not well encoded. */
#define ZFS_ERROR_HAS_DC_REPLY	-150	/* Code of last error which has
					   a DC_REPLY.  */

#define ZFS_EXITING		-151	/* zfsd is exiting */
#define ZFS_COULD_NOT_CONNECT	-152	/* Could not connect to node.  */
#define ZFS_COULD_NOT_AUTH	-153	/* Could not authenticate with node.  */
#define ZFS_CONNECTION_CLOSED	-154	/* Connection closed while waiting for
					   reply.  */
#define ZFS_REQUEST_TIMEOUT	-155	/* Request has timed out.  */

typedef enum direction_def
{
  DIR_REQUEST,		/* Request which wants a reply.  */
  DIR_REPLY,		/* Reply to request.  */
  DIR_ONEWAY,		/* Request which does not want a reply.  */
  DIR_LAST_AND_UNUSED
} direction;

typedef struct data_buffer_def
{
  uint32_t len;
#ifdef __KERNEL__
  const char __user *buf;
#else
  char *buf;
#endif
} data_buffer;

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

/* Connection speed.  */
typedef enum connection_speed_def
{
  CONNECTION_SPEED_NONE = 0,
  CONNECTION_SPEED_SLOW,
  CONNECTION_SPEED_FAST,
  CONNECTION_SPEED_LAST_AND_UNUSED
} connection_speed;

typedef struct zfs_fh_def
{
  uint32_t sid;		/* Server ID.  */
  uint32_t vid;		/* Volume ID.  */
  uint32_t dev;		/* Device ... */
  uint32_t ino;		/* ... and inode number of the file.  */
  uint32_t gen;		/* Generation of the file.  */
} zfs_fh;

typedef struct zfs_cap_def
{
  zfs_fh fh;
  uint32_t flags;
  char verify[ZFS_VERIFY_LEN];
} zfs_cap;

typedef uint32_t zfs_time;

typedef struct fattr_def
{
  uint32_t dev;
  uint32_t ino;
  uint64_t version;
  ftype type;
  uint32_t mode;
  uint32_t nlink;
  uint32_t uid;
  uint32_t gid;
  uint32_t rdev;
  uint64_t size;
  uint64_t blocks;
  uint32_t blksize;
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
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
  uint64_t size;
  zfs_time atime;
  zfs_time mtime;
} sattr;

typedef string filename;

typedef string zfs_path;

typedef string nodename;

typedef struct volume_root_args_def
{
  uint32_t vid;
} volume_root_args;

typedef struct setattr_args_def
{
  zfs_fh file;
  sattr attr;
} setattr_args;

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
  uint32_t flags;
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
  uint32_t flags;
} open_args;

typedef struct read_dir_args_def
{
  zfs_cap cap;
  int32_t cookie;
  uint32_t count;
} read_dir_args;

typedef struct dir_entry_def
{
  uint32_t ino;
  int32_t cookie;
  filename name;
} dir_entry;

typedef struct dir_list_def
{
  uint32_t n;
  char eof;
  void *buffer;			/* For internal use in zfsd.  */
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
  uint32_t count;
} read_args;

typedef struct read_res_def
{
  data_buffer data;
  uint64_t version;
} read_res;

typedef struct write_args_def
{
  zfs_cap cap;
  uint64_t offset;
  data_buffer data;
} write_args;

typedef struct write_res_def
{
  uint32_t written;
  uint64_t version;
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
  uint32_t rdev;
} mknod_args;

typedef struct auth_stage1_args_def
{
  nodename node;
} auth_stage1_args;

typedef struct auth_stage1_res_def
{
  nodename node;
} auth_stage1_res;

typedef struct auth_stage2_args_def
{
  connection_speed speed;
} auth_stage2_args;

typedef struct md5sum_args_def
{
  zfs_cap cap;
  uint32_t count;
  uint64_t offset[ZFS_MAX_MD5_CHUNKS];
  uint32_t length[ZFS_MAX_MD5_CHUNKS];
} md5sum_args;

typedef struct md5sum_res_def
{
  uint32_t count;
  uint32_t padding0;	/* workaround GDB bug */
  uint64_t size;
  uint64_t version;
  uint64_t offset[ZFS_MAX_MD5_CHUNKS];
  uint32_t length[ZFS_MAX_MD5_CHUNKS];
  unsigned char md5sum[ZFS_MAX_MD5_CHUNKS][MD5_SIZE];
} md5sum_res;

typedef struct file_info_res_def
{
  zfs_path path;
} file_info_res;

typedef struct reintegrate_args_def
{
  zfs_fh fh;
  char status;
} reintegrate_args;

typedef struct reintegrate_add_args_def
{
  zfs_fh fh;
  zfs_fh dir;
  filename name;
} reintegrate_add_args;

typedef struct reintegrate_del_args_def
{
  zfs_fh fh;
  zfs_fh dir;
  filename name;
  char destroy_p;
} reintegrate_del_args;

typedef struct reintegrate_set_args_def
{
  zfs_fh fh;
  uint64_t version;
} reintegrate_set_args;

typedef struct invalidate_args_def
{
  zfs_fh fh;
} invalidate_args;

typedef struct reread_config_args_def
{
  zfs_path path;
} reread_config_args;

typedef union call_args_def
{
  char null;
  char root;
  volume_root_args volume_root;
  zfs_fh getattr;
  setattr_args setattr;
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
  md5sum_args md5sum;
  data_buffer ping;
  zfs_fh file_info;
  reintegrate_add_args reintegrate_add;
  reintegrate_del_args reintegrate_del;
  reintegrate_set_args reintegrate_set;
  invalidate_args invalidate;
  reread_config_args reread_config;
  reintegrate_args reintegrate;
} call_args;

/* Mapping file type -> file mode.  */
extern unsigned int ftype2mode[FT_LAST_AND_UNUSED];

/* Function numbers.  */
enum function_number_def
{
#define ZFS_CALL_CLIENT
#define ZFS_CALL_SERVER
#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  ZFS_PROC_##NAME = NUMBER,
#include "zfs_prot.def"
  ZFS_PROC_LAST_AND_UNUSED
};
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL
#undef ZFS_CALL_SERVER
#undef ZFS_CALL_CLIENT

struct thread_def;
struct node_def;

#include "data-coding.h"

#ifdef __KERNEL__

#define ZFS_CALL_CLIENT
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  extern int zfs_proc_##FUNCTION##_zfsd(DC **dc, ARGS *args);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_CLIENT

#else /* __KERNEL__ */

#define ZFS_CALL_SERVER
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  extern void zfs_proc_##FUNCTION##_server (ARGS *args,			\
					    DC *dc, void *data,		\
					    bool map_id);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_SERVER

#define ZFS_CALL_CLIENT
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  extern int32_t zfs_proc_##FUNCTION##_client (struct thread_def *t,	\
					       ARGS *args,		\
					       struct node_def *nod,	\
					       int *fd);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_CLIENT

#define ZFS_CALL_CLIENT
#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  extern int32_t zfs_proc_##FUNCTION##_client_1 (struct thread_def *t,	\
						 ARGS *args, int fd);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL
#undef ZFS_CALL_CLIENT

#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  extern int32_t zfs_proc_##FUNCTION##_kernel (struct thread_def *t,	\
					       ARGS *args);
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL

/* Call statistics.  */
#define CALL_FROM_KERNEL	0
#define CALL_FROM_NETWORK	1
extern uint64_t call_statistics[2][ZFS_PROC_LAST_AND_UNUSED];

extern char *zfs_strerror (int32_t errnum);
extern void initialize_zfs_prot_c (void);
extern void cleanup_zfs_prot_c (void);

#endif /* !__KERNEL__ */

#endif
