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

typedef enum direction_def
{
  ZFS_REQUEST,
  ZFS_REPLY
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
  FT_FIFO
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
