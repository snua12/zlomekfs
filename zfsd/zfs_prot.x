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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

const ZFS_PORT       = 12323;
/* 12323 = ('Z' - 'A') + ('F' - 'A') * 26 + ('S' - 'A') * 26 * 26 */
const ZFS_MAXDATA    = 8192;
const ZFS_MAXPATHLEN = 1024;
const ZFS_MAXNAMELEN = 255;
const ZFS_FHSIZE     = 32;

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
  opaque data[ZFS_FHSIZE];
};

/* Timeval.  */ 
struct zfs_time
{
  unsigned sec;
  unsigned usec;
};

/* File attributes.  */
struct fattr
{
  enum ftype type;	/* file type */
  unsigned mode;	/* protection mode */
  unsigned nlink;	/* number of hard links */
  unsigned uid;		/* user ID of owner */
  unsigned gid;		/* group ID of owner */
  unsigned rdev;	/* device type (if inode device) */
  uint64_t size;	/* total size in bytes */
  uint64_t blocks;	/* number of blocks allocated */
  unsigned blksize;	/* filesystem block size */
  unsigned generation;	/* generation number of file */
  uint64_t fversion;	/* version number of file */
  unsigned sid;		/* server id */
  unsigned fsid;	/* device number */
  unsigned fileid;	/* inode number */
  zfs_time atime;	/* time of last access */
  zfs_time mtime;	/* time of last modification */
  zfs_time ctime;	/* time of last change */
};

/* File attributes that can be set.  */
struct sattr
{
  uint64_t size;	/* file size in bytes */
  unsigned mode;	/* protection mode */
  unsigned uid;		/* owner user id */
  unsigned gid;		/* owner group id */
};

typedef string filename<ZFS_MAXNAMELEN>;
typedef string zfs_path<ZFS_MAXPATHLEN>;

/* Reply status for file attributes.  */
union attrstat
switch (int status)
{
  case 0:
    fattr attributes;
  default:
    void;
};

/* Arguments of remote read.  */
struct readargs {
  zfs_fh file;		/* handle for file */
  uint64_t offset;	/* byte offset in file */
  unsigned count;	/* number of bytes to be read */
};

/* Reply of remote read.  */
struct readokres {
  opaque data<ZFS_MAXDATA>;
};

union readres
switch (int status)
{
  case 0:
    readokres reply;
  default:
    void;
};

/* Arguments of remote write. */
struct writeargs {
  zfs_fh file;		/* handle for file */
  uint64_t offset;	/* byte offset in file */
  unsigned count;	/* number of bytes to be read */
  opaque data<ZFS_MAXDATA>;
};

program ZFS_PROGRAM {
  version ZFS_VERSION {
    
    void
    ZFSPROC_NULL(void) = 0;

    attrstat
    ZFSPROC_GETATTR(zfs_fh) = 1;

    /* ??? ZFSPROC_OPEN(???) = 2;*/
    /* ??? ZFSPROC_OPEN_BY_FD(???) = 2;*/

    readres
    ZFSPROC_READ(readargs) = 3;

    /* ??? ZFSPROC_WRITE(???) = 4;*/
    /* ??? ZFSPROC_CLOSE(???) = 5;*/ 
    /* ??? ZFSPROC_LOOKUP(???) = 6;*/
    /* ??? ZFSPROC_SETATTR(???) = 7;*/
    /* ??? ZFSPROC_READLINK(???) = 7;*/
    /* ??? ZFSPROC_REMOVE(???) = 7;*/
    /* ??? ZFSPROC_RENAME(???) = 7;*/
    /* ??? ZFSPROC_LINK(???) = 7;*/
    /* ??? ZFSPROC_SYMLINK(???) = 7;*/
    /* ??? ZFSPROC_MKDIR(???) = 7;*/
    /* ??? ZFSPROC_RMDIR(???) = 7;*/
    /* ??? ZFSPROC_READDIR(???) = 7;*/
    /* ??? ZFSPROC_MKNOD(???) = 7;*/
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

