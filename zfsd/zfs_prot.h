#ifndef ZFS_PROT_H
#define ZFS_PROT_H

#include "system.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZFS_PORT 12323
#define ZFS_MAXDATA 8192
#define ZFS_MAXPATHLEN 1023
#define ZFS_MAXNAMELEN 255

struct data_buffer_def {
	unsigned int len;
	char buf[ZFS_MAXDATA];
};
typedef struct data_buffer_def data_buffer;

struct string_def {
	unsigned int len;
	char *buf;
};
typedef struct string_def string;

enum ftype_def {
	FT_BAD = 0,
	FT_REG = 1,
	FT_DIR = 2,
	FT_LNK = 3,
	FT_BLK = 4,
	FT_CHR = 5,
	FT_SOCK = 6,
	FT_FIFO = 7
};
typedef enum ftype_def ftype;

struct zfs_fh_def {
	unsigned int sid;
	unsigned int vid;
	unsigned int dev;
	unsigned int ino;
};
typedef struct zfs_fh_def zfs_fh;

struct zfs_time_def {
	unsigned int sec;
	unsigned int usec;
};
typedef struct zfs_time_def zfs_time;

struct fattr_def {
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
};
typedef struct fattr_def fattr;

struct sattr_def {
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	uint64_t size;
	zfs_time atime;
	zfs_time mtime;
};
typedef struct sattr_def sattr;

typedef string filename;

typedef string zfs_path;

struct sattr_args_def {
	zfs_fh file;
	sattr attributes;
};
typedef struct sattr_args_def sattr_args;

struct dir_op_args_def {
	zfs_fh dir;
	filename name;
};
typedef struct dir_op_args_def dir_op_args;

struct dir_op_res_def {
	zfs_fh file;
	fattr attributes;
};
typedef struct dir_op_res_def dir_op_res;

struct open_name_args_def {
	dir_op_args where;
	int flags;
	sattr attributes;
};
typedef struct open_name_args_def open_name_args;

struct open_res_def {
	zfs_fh file;
};
typedef struct open_res_def open_res;

struct read_dir_args_def {
	zfs_fh dir;
	unsigned int cookie;
	unsigned int count;
};
typedef struct read_dir_args_def read_dir_args;

struct entry_def {
	zfs_fh fh;
	filename name;
	unsigned int cookie;
	struct entry_def *next_entry;
};
typedef struct entry_def entry;

struct dir_list_def {
	entry *entries;
	char eof;
};
typedef struct dir_list_def dir_list;

struct read_dir_res_def {
	dir_list reply;
};
typedef struct read_dir_res_def read_dir_res;

struct rename_args_def {
	dir_op_args from;
	dir_op_args to;
};
typedef struct rename_args_def rename_args;

struct link_args_def {
	zfs_fh from;
	dir_op_args to;
};
typedef struct link_args_def link_args;

struct read_args_def {
	zfs_fh file;
	uint64_t offset;
	unsigned int count;
};
typedef struct read_args_def read_args;

struct read_res_def {
	data_buffer data;
};
typedef struct read_res_def read_res;

struct write_args_def {
	zfs_fh file;
	uint64_t offset;
	data_buffer data;
};
typedef struct write_args_def write_args;

struct write_res_def {
	unsigned int written;
};
typedef struct write_res_def write_res;

struct read_link_res_def {
	zfs_path path;
};
typedef struct read_link_res_def read_link_res;

struct symlink_args_def {
	dir_op_args from;
	zfs_path to;
	sattr attributes;
};
typedef struct symlink_args_def symlink_args;

struct mknod_args_def {
	dir_op_args where;
	sattr attributes;
	unsigned int rdev;
};
typedef struct mknod_args_def mknod_args;

#ifdef __cplusplus
}
#endif

#endif /* !_ZFS_PROT_H_RPCGEN */
