/* File operations.
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

#include "system.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "fh.h"
#include "file.h"
#include "dir.h"

/* Get attributes for file with handle FH and store them to FA.  */

int
zfs_getattr (fattr *fa, zfs_fh *fh)
{
  volume vol;
  internal_fh ifh;
  virtual_dir vd;
  
  /* Lookup the FH.  */
  if (!fh_lookup (fh, &vol, &ifh, &vd))
    return ESTALE;

  if (vd)
    {
      memcpy (fa, &vd->attr, sizeof (fattr));
    }
  else /* if (ifh) */
    {
      char *path;
      struct stat st;
      int r;

      path = build_local_path (vol, ifh);
      r = lstat (path, &st);
      free (path);
      if (r != 0)
	return errno;

      switch (st.st_mode & S_IFMT)
	{
	  case S_IFSOCK:
	    fa->type = FT_SOCK;
	    break;

	  case S_IFLNK:
	    fa->type = FT_LNK;
	    break;

	  case S_IFREG:
	    fa->type = FT_REG;
	    break;

	  case S_IFBLK:
	    fa->type = FT_BLK;
	    break;

	  case S_IFDIR:
	    fa->type = FT_DIR;
	    break;

	  case S_IFCHR:
	    fa->type = FT_CHR;
	    break;

	  case S_IFIFO:
	    fa->type = FT_FIFO;
	    break;

	  default:
	    fa->type = FT_BAD;
	    break;
	}

      fa->mode = st.st_mode & (S_IRWXU | S_ISUID | S_ISGID | S_ISVTX);
      fa->nlink = st.st_nlink;
      fa->uid = st.st_uid;  /* FIXME: tarnslate UID */
      fa->gid = st.st_gid;  /* FIXME: tarnslate GID */
      fa->rdev = st.st_rdev;
      fa->size = st.st_size;
      fa->blocks = st.st_blocks;
      fa->blksize = st.st_blksize;
      fa->generation = 0;	/* FIXME? how? */
      fa->fversion = 0;		/* FIXME */
      fa->sid = ifh->local_fh.sid;
      fa->vid = ifh->local_fh.vid;
      fa->fsid = ifh->local_fh.dev;
      fa->fileid = ifh->local_fh.ino;
      fa->atime = st.st_atime;
      fa->mtime = st.st_mtime;
      fa->ctime = st.st_ctime;
    }

  return ZFS_OK;
}

/*****************************************************************************/

int
zfs_open (zfs_fh *fh)
{

  return ZFS_OK;
}

int
zfs_open_by_name (zfs_fh *fh, zfs_fh *dir, const char *name, int flags,
		  sattr *attr)
{
  int r;

  if (!(flags & O_CREAT))
    {
      r = zfs_lookup (fh, dir, name);
      if (r)
	return r;

      return zfs_open (fh);
    }
  else
    {
      /* FIXME: finish */
    }
  return ZFS_OK;
}

int
zfs_setattr (fattr *fa, zfs_fh *fh, unsigned int valid, sattr *sa)
{
  return ZFS_OK;
}

int
zfs_close (zfs_fh *fh)
{
  return ZFS_OK;
}
