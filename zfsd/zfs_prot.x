/* ZFS protocol definition.
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

const ZFS_PORT       = 12323;
/* 12323 = ('Z' - 'A') + ('F' - 'A') * 26 + ('S' - 'A') * 26 * 26 */
const ZFS_MAXDATA    = 8192;
const ZFS_MAXPATHLEN = 1024;
const ZFS_MAXNAMELEN = 255;
const ZFS_FHSIZE     = 16;

/* File types.  */
enum ftype
{
  FT_BAD = 0,	/* unused */
  FT_REG = 1,	/* regular file */
  FT_DIR = 2,	/* directory */
  FT_LNK = 3,	/* symbolic link */
  FT_BLK = 4,	/* block device */
  FT_CHR = 5,	/* char device */
  FT_SOCK = 6,	/* unix domain socket */
  FT_FIFO = 7	/* named pipe */
};

/* File handle.  */
struct zfs_fh
{
  opaque data[ZFS_FHSIZE];	/* file handle data */
};

/* Timeval.  */ 
struct zfs_time
{
  unsigned sec;			/* seconds */
  unsigned usec;		/* microseconds */
};

/* File attributes.  */
struct fattr
{
  enum ftype type;		/* file type */
  unsigned mode;		/* protection mode */
  unsigned nlink;		/* number of hard links */
  unsigned uid;			/* user ID of owner */
  unsigned gid;			/* group ID of owner */
  unsigned rdev;		/* device type (if inode device) */
  uint64_t size;		/* total size in bytes */
  uint64_t blocks;		/* number of blocks allocated */
  unsigned blksize;		/* filesystem block size */
  unsigned generation;		/* ??? - generation number of file */
  uint64_t fversion;		/* version number of file */
  unsigned sid;			/* ??? - server id */
  unsigned vid;			/* ??? - volume id */
  unsigned fsid;		/* ??? - device number */
  unsigned fileid;		/* ??? - inode number */
  zfs_time atime;		/* time of last access */
  zfs_time mtime;		/* time of last modification */
  zfs_time ctime;		/* time of last change */
};

#define ZFS_ATTR_MODE	1
#define ZFS_ATTR_UID	2
#define ZFS_ATTR_GID	4
#define ZFS_ATTR_SIZE	8
#define ZFS_ATTR_ATIME	16
#define ZFS_ATTR_MTIME	32

/* File attributes that can be set.  */
struct sattr
{
  unsigned mode;		/* protection mode */
  unsigned uid;			/* owner user id */
  unsigned gid;			/* owner group id */
  uint64_t size;		/* file size in bytes */
  zfs_time atime;		/* time of last access */
  zfs_time mtime;		/* time of last modification */
};

/* File name and path name.  */
typedef string filename<ZFS_MAXNAMELEN>;
typedef string zfs_path<ZFS_MAXPATHLEN>;

/* Reply status of file attributes.  */
union attr_res
switch (int status)
{
  case 0:
    fattr attributes;		/* file attributes */
  default:
    void;
};

/* Arguments of setattr.  */
struct sattr_args
{
  zfs_fh file;			/* handle of file */
  unsigned valid;		/* flags which parts of sattr are valid */
  sattr attributes;		/* file attributes */
};

/* Arguments of some directory operations.  */
struct dir_op_args
{
  zfs_fh dir;			/* directory file handle */
  filename name;		/* name */
};

/* Reply of some directory operations.  */
struct dir_op_ok_res
{
  zfs_fh file;			/* handle of file */
  fattr attributes;		/* file attributes */
};

union dir_op_res
switch (int status)
{
  case 0:
    dir_op_ok_res res;		/* handle and attributes of the file */
  default:
    void;
};

/* Arguments of open_by_name, mkdir... */
struct open_name_args
{
  dir_op_args where;		/* file handle of (parent) dir + name */
  int flags;			/* open flags */
  sattr attributes;		/* attributes of new file / dir */
};

/* Reply of open.  */
union open_res
switch (int status)
{
  case 0:
    zfs_fh fh;			/* handle of file */
  default:
    void;
};

/* Arguments of readdir.  */
struct read_dir_args
{
  zfs_fh dir;			/* handle of directory */
  unsigned cookie;		/* cookie from the last entry returned
				   by last readdir operation */
  unsigned count;		/* number of directory bytes to read */
};

/* Reply of readdir.  */
#if 1
struct entry
{
  zfs_fh fh;			/* handle of file */
  filename name;		/* name of file */
  unsigned cookie;		/* cookie for next readdir operation */
};

struct dir_list
{
  unsigned n;			/* number of entries */
  bool eof;			/* have we reached the end of directory? */
  entry entries[1];		/* array of entries */
};
#else
struct entry
{
  zfs_fh fh;			/* handle of file */
  filename name;		/* name of file */
  unsigned cookie;		/* cookie for next readdir operation */
  struct entry *next_entry;	/* pointer to next entry */
};

struct dir_list
{
  entry *entries;		/* link list of entries */
  bool eof;			/* have we reached the end of directory? */
};
#endif

union read_dir_res
switch (int status)
{
  case 0:
    dir_list reply;		/* list of dir entries */
  default:
    void;
};

/* Arguments of rename.  */
struct rename_args
{
  dir_op_args from;		/* file handle of source dir + name */
  dir_op_args to;		/* file handle of destination dir + name */
};

/* Arguments of link.  */
struct link_args
{
  zfs_fh from;			/* file handle of the source file */
  dir_op_args to;		/*  file handle of destination dir + name */
};

/* Arguments of read.  */
struct read_args
{
  zfs_fh file;			/* handle of file */
  uint64_t offset;		/* byte offset in file */
  unsigned count;		/* number of bytes to be read */
};

/* Reply of read.  */
union read_res
switch (int status)
{
  case 0:
    opaque data<ZFS_MAXDATA>;	/* data including the length */
  default:
    void;
};

/* Arguments of write. */
struct write_args
{
  zfs_fh file;			/* handle of file */
  uint64_t offset;		/* byte offset in file */
  opaque data<ZFS_MAXDATA>;	/* data including the length */
};

/* Reply from write.  */
union write_res
switch (int status)
{
  case 0:
    unsigned written;		/* number of bytes written */
  default:
    void;
};

/* Reply of readlink.  */
union read_link_res
switch (int status)
{
  case 0:
    zfs_path path;		/* data of symlink */
  default:
    void;
};

/* Arguments of symlink.  */
struct symlink_args
{
  dir_op_args from;		/* source of symlink (new path) */
  zfs_path to;			/* destination of symlink (old path) */
  sattr attributes;		/* attributes of symlink */
};

/* Arguments of mknod.  */
struct mknod_args
{
  dir_op_args where;		/* handle of destination dir + name */
  sattr attributes;		/* attributes of device */
  unsigned rdev;		/* device number */
};

program ZFS_PROGRAM {
  version ZFS_VERSION {
    void
    ZFSPROC_NULL(void) = 0;

    attr_res
    ZFSPROC_GETATTR(zfs_fh) = 1;

    attr_res
    ZFSPROC_SETATTR(sattr_args) = 2;

    dir_op_res
    ZFSPROC_LOOKUP(dir_op_args) = 3;

    open_res
    ZFSPROC_OPEN_BY_NAME(open_name_args) = 4;

    open_res
    ZFSPROC_OPEN_BY_FD(zfs_fh) = 5;

    int
    ZFSPROC_CLOSE(zfs_fh) = 15;

    read_dir_res
    ZFSPROC_READDIR(read_dir_args) = 6;
    
    dir_op_res
    ZFSPROC_MKDIR(open_name_args) = 7;
    
    int
    ZFSPROC_RMDIR(dir_op_args) = 8;

    int
    ZFSPROC_RENAME(rename_args) = 9;

    int
    ZFSPROC_LINK(link_args) = 10;

    int 
    ZFSPROC_UNLINK(dir_op_args) = 11;

    read_res
    ZFSPROC_READ(read_args) = 16;

    write_res
    ZFSPROC_WRITE(write_args) = 17;
    
    read_link_res
    ZFSPROC_READLINK(zfs_fh) = 18;

    int
    ZFSPROC_SYMLINK(symlink_args) = 19;

    int
    ZFSPROC_MKNOD(mknod_args) = 20;

    /* ??? ZFSPROC_STATVOLUME???(???) = 7;*/
#if 0
CODA:
	"vget        ",   /* 22 */
	"signal      ",   /* 23 */
	"replace     ",   /* 24 */
	"flush       ",   /* 25 */
	"purgeuser   ",   /* 26 */
	"zapfile     ",   /* 27 */
	"zapdir      ",   /* 28 */
	"-           ",   /* 29 */
	"purgefid    ",   /* 30 */
	"open_by_path",   /* 31 */
	"resolve     ",   /* 32 */
	"reintegrate ",   /* 33 */
	"statfs      ",   /* 34 */
	"store       ",   /* 35 */
	"release     "    /* 36 */
#endif

  } = 1;
} = 0x335a4653;

