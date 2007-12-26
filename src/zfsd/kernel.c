/*! \file
    \brief Functions for threads communicating with kernel.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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
#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "pthread.h"
#include "constant.h"
#include "semaphore.h"
#include "hashtab.h"
#include "alloc-pool.h"
#include "data-coding.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "network.h"
#include "log.h"
#include "node.h"
#include "util.h"
#include "memory.h"
#include "thread.h"
#include "user-group.h"
#include "zfs-prot.h"
#include "config.h"

/* In seconds before a revalidation is required */
#define CACHE_VALIDITY 5

/*! Arguments from main (), after parsing the zfsd-specific options */
struct fuse_args main_args;

/*! Pool of kernel threads (threads communicating with kernel).  */
thread_pool kernel_pool;

/*! Is ZFS mounted?  */
bool mounted = false;

/*! FUSE kernel communication channel */
static struct fuse_chan *fuse_ch;
/*! FUSE mount session */
static struct fuse_session *fuse_se;
/*! The mount point */
static char *fuse_mountpoint;

 /* Inode <-> file handle mapping */

struct inode_map
{
  fuse_ino_t ino;
  zfs_fh fh;
};

static fuse_ino_t next_ino;

static htab_t inode_map_ino, inode_map_fh;
static pthread_mutex_t inode_map_mutex;

static hash_t
inode_map_ino_hash (const void *x)
{
  return ((const struct inode_map *)x)->ino;
}

static hash_t
inode_map_fh_hash (const void *x)
{
  return ZFS_FH_HASH (&((const struct inode_map *)x)->fh);
}

static int
inode_map_ino_eq (const void *x, const void *y)
{
  return ((const struct inode_map *)x)->ino == *(const fuse_ino_t *)y;
}

static int
inode_map_fh_eq (const void *x, const void *y)
{
  return ZFS_FH_EQ (((const struct inode_map *)x)->fh, *(const zfs_fh *)y);
}

/* Return 0 if not found */
static fuse_ino_t
fh_get_inode (const zfs_fh *fh)
{
  struct inode_map *map;

  zfsd_mutex_lock (&inode_map_mutex);
  map = htab_find_with_hash (inode_map_fh, fh, ZFS_FH_HASH (fh));
  zfsd_mutex_unlock (&inode_map_mutex);
  if (map != NULL)
    return map->ino;
  else
    return 0;
}

/* FIXME?
   This means that (find zfs_root) will permanently pin a lot of memory... */
static fuse_ino_t
fh_to_inode (const zfs_fh *fh)
{
  void **slot;
  struct inode_map *map;

  zfsd_mutex_lock (&inode_map_mutex);
  slot = htab_find_slot_with_hash (inode_map_fh, fh, ZFS_FH_HASH (fh), INSERT);
  if (*slot != NULL)
    map = *slot;
  else
    {
      map = xmalloc (sizeof (*map));
      map->ino = next_ino;
      next_ino++;
      map->fh = *fh;
      *slot = map;
      slot = htab_find_slot_with_hash (inode_map_ino, &map->ino, map->ino,
				       INSERT);
      assert (*slot == NULL);
      *slot = map;
    }
  zfsd_mutex_unlock (&inode_map_mutex);
  return map->ino;
}

static const zfs_fh *
inode_to_fh (fuse_ino_t ino)
{
  struct inode_map *map;

  zfsd_mutex_lock (&inode_map_mutex);
  map = htab_find_with_hash (inode_map_ino, &ino, ino);
  zfsd_mutex_unlock (&inode_map_mutex);
  if (map != NULL)
    return &map->fh;
  else
    return NULL;
}

static void
inode_map_init (void)
{
  zfsd_mutex_init (&inode_map_mutex);
  inode_map_ino = htab_create (100, inode_map_ino_hash, inode_map_ino_eq, NULL,
			       NULL);
  inode_map_fh = htab_create (100, inode_map_fh_hash, inode_map_fh_eq, NULL,
			      NULL);
  next_ino = FUSE_ROOT_ID;
}

 /* Global connection data */

static pthread_mutex_t buffer_pool_mutex;
static void *buffer_pool[MAX_FREE_DCS];
static size_t buffer_pool_size; /* = 0; */

/*! Unmount the FUSE mountpoint and destroy data structures used by it.  */

void
kernel_unmount (void)
{
  if (!mounted)
    return;

  fuse_session_remove_chan (fuse_ch);
  fuse_session_destroy (fuse_se);
  fuse_unmount (fuse_mountpoint, fuse_ch);
  mounted = false;
}

 /* Data translation */

/*! Mapping file type -> d_type value.  */
static const unsigned char ftype2dtype[FT_LAST_AND_UNUSED]
  = {DT_UNKNOWN, DT_REG, DT_DIR, DT_LNK, DT_BLK, DT_CHR, DT_SOCK, DT_FIFO};

static ftype
ftype_from_mode_t (mode_t mode)
{
  switch (mode & S_IFMT)
    {
    case S_IFREG:
      return FT_REG;

    case S_IFDIR:
      return FT_DIR;

    case S_IFLNK:
      return FT_LNK;

    case S_IFBLK:
      return FT_BLK;

    case S_IFCHR:
      return FT_CHR;

    case S_IFSOCK:
      return FT_SOCK;

    case S_IFIFO:
      return FT_FIFO;

    default:
      return FT_BAD;
    }
}

/* Typically the caller will want to use a different attr->mode */
static void
sattr_from_req (sattr *attr, fuse_req_t req)
{
  const struct fuse_ctx *ctx;

  ctx = fuse_req_ctx (req);
  attr->mode = -1;
  attr->uid = map_uid_node2zfs (ctx->uid);
  attr->gid = map_gid_node2zfs (ctx->gid);
  attr->size = -1;
  attr->atime = -1;
  attr->mtime = -1;
}

static void
stat_from_fattr (struct stat *st, const fattr *fa, fuse_ino_t ino)
{
  memset (st, 0, sizeof (*st));
  st->st_ino = ino;
  st->st_mode = ftype2mode[fa->type] | fa->mode;
  st->st_nlink = fa->nlink;
  st->st_uid = map_uid_zfs2node (fa->uid);
  st->st_gid = map_gid_zfs2node (fa->gid);
  st->st_rdev = fa->rdev;
  st->st_size = fa->size;
  st->st_blksize = fa->blksize;
  st->st_blocks = fa->blocks;
  st->st_atime = fa->atime;
  st->st_mtime = fa->mtime;
  st->st_ctime = fa->ctime;
}

static void
entry_from_dir_op_res (struct fuse_entry_param *e, const dir_op_res *res)
{
  e->ino = fh_to_inode (&res->file);
  e->generation = res->file.gen;
  stat_from_fattr (&e->attr, &res->attr, e->ino);
  e->attr_timeout = CACHE_VALIDITY;
  e->entry_timeout = CACHE_VALIDITY;
}

 /* Request translation */

static void
zfs_fuse_lookup (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  dir_op_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  err = -zfs_error (zfs_lookup (&res, &args.dir, &args.name));
  free (args.name.str);
  if (err != 0)
    goto err_estale;
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_getattr (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  struct stat st;
  const zfs_fh *fh;
  zfs_fh args;
  fattr fa;
  int err;

  (void)fi;
  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args = *fh;
  err = -zfs_error (zfs_getattr (&fa, &args));
  if (err != 0)
    goto err; /* ESTALE not handled specially */
  stat_from_fattr (&st, &fa, ino);
  fuse_reply_attr (req, &st, CACHE_VALIDITY);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_setattr (fuse_req_t req, fuse_ino_t ino, struct stat *attr,
		  int to_set, struct fuse_file_info *fi)
{
  struct stat st;
  const zfs_fh *fh;
  setattr_args args;
  fattr fa;
  int err;

  (void)fi;
  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.file = *fh;
  if ((to_set & FUSE_SET_ATTR_MODE) != 0)
    args.attr.mode = attr->st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU
				      | S_IRWXG | S_IRWXO);
  else
    args.attr.mode = -1;
  if ((to_set & FUSE_SET_ATTR_UID) != 0)
    args.attr.uid = map_uid_node2zfs(attr->st_uid);
  else
    args.attr.uid = -1;
  if ((to_set & FUSE_SET_ATTR_GID) != 0)
    args.attr.gid = map_gid_node2zfs (attr->st_gid);
  else
    args.attr.gid = -1;
  /* FIXME: verify ftruncate() works correctly */
  if ((to_set & FUSE_SET_ATTR_SIZE) != 0)
    args.attr.size = attr->st_size;
  else
    args.attr.size = -1;
  if ((to_set & FUSE_SET_ATTR_ATIME) != 0)
    args.attr.atime = attr->st_atime; /* FIXME: round subsecond time up? */
  else
    args.attr.atime = -1;
  if ((to_set & FUSE_SET_ATTR_MTIME) != 0)
    args.attr.mtime = attr->st_mtime; /* FIXME: round subsecond time up? */
  else
    args.attr.mtime = -1;
  err = -zfs_error (zfs_setattr (&fa, &args.file, &args.attr));
  if (err != 0)
    goto err_estale;
  stat_from_fattr (&st, &fa, ino);
  fuse_reply_attr (req, &st, CACHE_VALIDITY);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata (fuse_se, ino);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_readlink (fuse_req_t req, fuse_ino_t ino)
{
  const zfs_fh *fh;
  zfs_fh args;
  read_link_res res;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args = *fh;
  err = -zfs_error (zfs_readlink (&res, &args));
  if (err != 0)
    goto err;
  fuse_reply_readlink (req, res.path.str);
  free (res.path.str);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_mknod (fuse_req_t req, fuse_ino_t parent, const char *name,
		mode_t mode, dev_t rdev)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  mknod_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.where.dir = *fh;
  xmkstring (&args.where.name, name);
  sattr_from_req (&args.attr, req);
  args.attr.mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG
			   | S_IRWXO);
  args.type = ftype_from_mode_t (mode); /* Note that type may be FT_REG */
  if (args.type == FT_BAD)
    {
      message (LOG_WARNING, FACILITY_DATA, "Invalid file type in mknod\n");
      free (args.where.name.str);
      err = EINVAL;
      goto err;
    }
  args.rdev = rdev;
  err = -zfs_error (zfs_mknod (&res, &args.where.dir, &args.where.name,
			       &args.attr, args.type, args.rdev));
  free (args.where.name.str);
  if (err != 0)
    goto err_estale;
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_mkdir (fuse_req_t req, fuse_ino_t parent, const char *name,
		mode_t mode)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  mkdir_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.where.dir = *fh;
  xmkstring (&args.where.name, name);
  sattr_from_req (&args.attr, req);
  args.attr.mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG
			   | S_IRWXO);
  err = -zfs_error (zfs_mkdir (&res, &args.where.dir, &args.where.name,
			       &args.attr));
  free (args.where.name.str);
  if (err != 0)
    goto err_estale;
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_unlink (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  const zfs_fh *fh;
  dir_op_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  err = -zfs_error (zfs_unlink (&args.dir, &args.name));
  free (args.name.str);
  if (err != 0)
    goto err_estale;
  goto err;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_rmdir (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  const zfs_fh *fh;
  dir_op_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  err = -zfs_error (zfs_rmdir (&args.dir, &args.name));
  free (args.name.str);
  if (err != 0)
    goto err_estale;
  goto err;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_symlink (fuse_req_t req, const char *dest, fuse_ino_t parent,
		  const char *name)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  symlink_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.from.dir = *fh;
  xmkstring (&args.from.name, name);
  xmkstring (&args.to, dest);
  sattr_from_req (&args.attr, req);
  err = -zfs_error (zfs_symlink (&res, &args.from.dir, &args.from.name,
				 &args.to, &args.attr));
  free (args.from.name.str);
  free (args.to.str);
  if (err != 0)
    goto err_estale;
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_rename (fuse_req_t req, fuse_ino_t parent, const char *name,
		 fuse_ino_t newparent, const char *newname)
{
  const zfs_fh *fh;
  rename_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.from.dir = *fh;
  fh = inode_to_fh (newparent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.to.dir = *fh;
  xmkstring (&args.from.name, name);
  xmkstring (&args.to.name, newname);
  err = -zfs_error (zfs_rename(&args.from.dir, &args.from.name, &args.to.dir,
			       &args.to.name));
  free (args.from.name.str);
  free (args.to.name.str);
  if (err != 0)
    goto err_estale;
  goto err;

 err_estale:
  if (err == ESTALE)
    {
      (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
      (void)fuse_kernel_invalidate_metadata(fuse_se, newparent);
    }
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_link (fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
	       const char *newname)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  link_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.from = *fh;
  fh = inode_to_fh (newparent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.to.dir = *fh;
  xmkstring (&args.to.name, newname);
  err = -zfs_error (zfs_link (&args.from, &args.to.dir, &args.to.name));
  if (err != 0)
    {
      free (args.to.name.str);
      goto err_estale;
    }
  err = -zfs_error (zfs_lookup (&res, &args.to.dir, &args.to.name));
  free (args.to.name.str);
  if (err != 0)
    goto err_estale_newparent;
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
 err_estale_newparent:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, newparent);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_open (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  const zfs_fh *fh;
  open_args args;
  zfs_cap res, *cap;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.file = *fh;
  args.flags = fi->flags;
  err = -zfs_error (zfs_open (&res, &args.file, args.flags));
  if (err != 0)
    goto err_estale;
  cap = xmalloc (sizeof (*cap));
  *cap = res;
  fi->fh = (intptr_t)cap;
  fi->direct_io = 0; /* Use the page cache */
  fi->keep_cache = 1;
  if (fuse_reply_open (req, fi) != 0)
    {
      (void)zfs_close (cap);
      free (cap);
    }
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_fuse_read (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	       struct fuse_file_info *fi)
{
  zfs_cap *cap;
  void *buf;
  size_t done;
  int err;

  cap = (zfs_cap *)(intptr_t)fi->fh;
  buf = xmalloc (size);
  done = 0;
  do
    {
      read_args args;
      read_res res;
      size_t run;

      run = size - done;
      if (run > ZFS_MAXDATA)
	run = ZFS_MAXDATA;
      args.cap = *cap;
      args.offset = off + done;
      args.count = run;
      res.data.buf = (char *)buf + done;
      err = -zfs_error (zfs_read (&res, &args.cap, args.offset, args.count,
				  true));
      if (err != 0)
	goto err_buf_estale;
      if (res.data.len == 0)
	break;
      done += res.data.len;
    }
  while (done < size);
  fuse_reply_buf (req, buf, done);
  free (buf);
  return;

 err_buf_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
  free (buf);
  fuse_reply_err (req, err);
}

static void
zfs_fuse_write (fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size,
		off_t off, struct fuse_file_info *fi)
{
  zfs_cap *cap;
  size_t done;
  int err;

  cap = (zfs_cap *)(intptr_t)fi->fh;
  done = 0;
  do
    {
      write_args args;
      write_res res;
      size_t run;

      run = size - done;
      if (run > ZFS_MAXDATA)
	run = ZFS_MAXDATA;
      args.cap = *cap;
      args.offset = off + done;
      args.data.len = run;
      args.data.buf = CAST_QUAL (char *, buf + done);
      err = -zfs_error (zfs_write (&res, &args));
      if (err != 0)
	goto err_estale;
      done += res.written;
    }
  while (done < size);
  fuse_reply_write (req, size);
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
   fuse_reply_err (req, err);
}

static void
zfs_fuse_release (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  zfs_cap *cap;
  int err;

  (void)fuse_kernel_sync_inode(fuse_se, ino);
  cap = (zfs_cap *)(intptr_t)fi->fh;
  err = -zfs_error (zfs_close (cap));
  free (cap);
  if (err != 0)
    goto err; /* ESTALE not handled specially */
  (void)fuse_kernel_invalidate_data(fuse_se, ino);
  /* Fall through */
 err:
  fuse_reply_err (req, err);
}

static void
free_dir_list_array (dir_list *list)
{
  dir_entry *entries;
  size_t i;

  entries = list->buffer;
  for (i = 0; i < list->n; i++)
    free (entries[i].name.str);
}

static void
zfs_fuse_readdir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		  struct fuse_file_info *fi)
{
  read_dir_args args;
  zfs_cap *cap;
  dir_list list;
  dir_entry entries[ZFS_MAX_DIR_ENTRIES];
  char *buf;
  size_t buf_offset;
  uint32_t i;
  int err;

  cap = (zfs_cap *)(intptr_t)fi->fh;
  args.cap = *cap;
  args.cookie = off;
  args.count = size < ZFS_MAXDATA ? size : ZFS_MAXDATA;

  list.n = 0;
  list.eof = 0;
  list.buffer = entries;
  err = -zfs_error (zfs_readdir (&list, &args.cap, args.cookie, args.count,
				 &filldir_array));
  if (err != 0)
    goto err_list_estale;

  buf = xmalloc (size);
  buf_offset = 0;
  for (i = 0; i < list.n; i++)
    {
      dir_op_args lookup_args;
      dir_op_res lookup_res;
      dir_entry *entry;
      struct stat st;
      size_t sz;

      entry = entries + i;
      /* Ugly.  The ino returned by zfs_readdir () is only a part of the FH,
	 from a different namespace than the kernel ino.  To get the kernel ino,
	 a lookup is necessary to get the full FH. */
      lookup_args.dir = cap->fh;
      xstringdup (&lookup_args.name, &entry->name);
      err = -zfs_error (zfs_lookup (&lookup_res, &lookup_args.dir,
				    &lookup_args.name));
      free (lookup_args.name.str);
      if (err != 0)
	continue;
      st.st_ino = fh_to_inode (&lookup_res.file);
      st.st_mode = ftype2dtype[lookup_res.attr.type];
      sz = fuse_add_direntry (req, buf + buf_offset, size - buf_offset,
			      entry->name.str, &st, entry->cookie);
      if (buf_offset + sz > size)
	break;
      buf_offset += sz;
    }
  fuse_reply_buf (req, buf, buf_offset);
  free (buf);
  free_dir_list_array (&list);
  return;

 err_list_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
  free_dir_list_array (&list);
  fuse_reply_err (req, err);
}

static void
zfs_fuse_statfs (fuse_req_t req, fuse_ino_t ino)
{
  struct statvfs sfs;

  (void)ino;
  memset (&sfs, 0, sizeof (sfs));
  sfs.f_bsize = ZFS_MAXDATA;
  sfs.f_frsize = 0;
  sfs.f_blocks = 0;
  sfs.f_bfree = 0;
  sfs.f_bavail = 0;
  sfs.f_files = 0;
  sfs.f_ffree = 0;
  sfs.f_favail = 0;
  sfs.f_fsid = 'z' | 'f' << 8 | 's' << 16;
  sfs.f_flag = 0;
  sfs.f_namemax = ZFS_MAXNAMELEN;
  fuse_reply_statfs (req, &sfs);
}

static void
zfs_fuse_create (fuse_req_t req, fuse_ino_t parent, const char *name,
		 mode_t mode, struct fuse_file_info *fi)
{
  struct fuse_entry_param e;
  const zfs_fh *fh;
  create_args args;
  create_res res;
  zfs_cap *cap;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EINVAL;
      goto err;
    }
  args.where.dir = *fh;
  xmkstring (&args.where.name, name);
  args.flags = fi->flags;
  sattr_from_req (&args.attr, req);
  args.attr.mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG
			   | S_IRWXO);
  err = -zfs_error (zfs_create (&res, &args.where.dir, &args.where.name,
				args.flags, &args.attr));
  free (args.where.name.str);
  if (err != 0)
    goto err_estale;
  entry_from_dir_op_res (&e, &res.dor);
  cap = xmalloc (sizeof (*cap));
  *cap = res.cap;
  fi->fh = (intptr_t)cap;
  fi->direct_io = 0; /* Use the page cache */
  fi->keep_cache = 1;
  if (fuse_reply_create (req, &e, fi) != 0)
    {
      (void)zfs_close (cap);
      free (cap);
    }
  return;

 err_estale:
  if (err == ESTALE)
    (void)fuse_kernel_invalidate_metadata(fuse_se, parent);
 err:
  fuse_reply_err (req, err);
}

static const struct fuse_lowlevel_ops zfs_fuse_ops =
  {
    .lookup = zfs_fuse_lookup,
    /* FIXME? .forget */
    .getattr = zfs_fuse_getattr,
    .setattr = zfs_fuse_setattr,
    .readlink = zfs_fuse_readlink,
    .mknod = zfs_fuse_mknod,
    .mkdir = zfs_fuse_mkdir,
    .unlink = zfs_fuse_unlink,
    .rmdir = zfs_fuse_rmdir,
    .symlink = zfs_fuse_symlink,
    .rename = zfs_fuse_rename,
    .link = zfs_fuse_link,
    .open = zfs_fuse_open,
    .read = zfs_fuse_read,
    .write = zfs_fuse_write,
    /* .flush not necessary */
    .release = zfs_fuse_release,
    /* .fsync not implemented */
    .opendir = zfs_fuse_open,
    .readdir = zfs_fuse_readdir,
    .releasedir = zfs_fuse_release,
    /* .fsyncdir not implemented */
    .statfs = zfs_fuse_statfs,
    /* .setxattr, .getxattr, .listxattr, .removexattr not implemented */
    /* .access not necessary, the filesystem should be mounted with
       -o default_permissions */
    .create = zfs_fuse_create,
    /* .getlk, .setlk not implemented */
    /* .bmap not applicable */
  };

int32_t
zfs_proc_invalidate_kernel (thread *t, invalidate_args *args)
{
  fuse_ino_t ino;

  if (!mounted)
    {
      t->retval = ZFS_COULD_NOT_CONNECT;
      goto err;
    }
  ino = fh_get_inode (&args->fh);
  if (ino != 0)
    (void)fuse_kernel_invalidate_metadata(fuse_se, ino);
  t->retval = ZFS_OK;
  /* Fall through */
 err:
  return t->retval;
}

 /* Thread glue */

/*! Initialize kernel thread T.  */

static void
kernel_worker_init (thread *t)
{
  t->dc_call = dc_create ();
}

/*! Cleanup kernel thread DATA.  */

static void
kernel_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (t->dc_call);
}

/*! The main function of the kernel thread.  */

static void *
kernel_worker (void *data)
{
  thread *t = (thread *) data;
  lock_info li[MAX_LOCKED_FILE_HANDLES];

  thread_disable_signals ();

  pthread_cleanup_push (kernel_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Kernel worker thread");
  set_lock_info (li);

  while (1)
    {
      /* Wait until kernel_dispatch wakes us up.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
        abort ();
#endif

      /* We were requested to die.  */
      if (get_thread_state (t) == THREAD_DYING)
        break;

      /* ZFS is mounted if kernel wants something from zfsd.  */
      mounted = true;

      fuse_session_process (fuse_se, t->u.kernel.buf, t->u.kernel.buf_size,
			    t->u.kernel.fuse_ch);

      zfsd_mutex_lock (&buffer_pool_mutex);
      if (buffer_pool_size < MAX_FREE_DCS)
	{
	  buffer_pool[buffer_pool_size] = t->u.kernel.buf;
	  buffer_pool_size++;
	}
      else
	free(t->u.kernel.buf);
      zfsd_mutex_unlock (&buffer_pool_mutex);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&kernel_pool.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
        {
          queue_put (&kernel_pool.idle, &t->index);
          set_thread_state (t, THREAD_IDLE);
        }
      else
        {
#ifdef ENABLE_CHECKING
          if (get_thread_state (t) != THREAD_DYING)
            abort ();
#endif
          zfsd_mutex_unlock (&kernel_pool.mutex);
          break;
        }
      zfsd_mutex_unlock (&kernel_pool.mutex);
    }

  pthread_cleanup_pop (1);

  return NULL;
}

/*! Function which gets a request and passes it to some kernel thread.
   It also regulates the number of kernel threads.  */
static void
kernel_dispatch (struct fuse_chan *ch, void *buf, size_t buf_size)
{
  size_t idx;

  zfsd_mutex_lock (&kernel_pool.mutex);

  /* Regulate the number of threads.  */
  if (kernel_pool.idle.nelem == 0)
    thread_pool_regulate (&kernel_pool);

  /* Select an idle thread and forward the request to it.  */
  queue_get (&kernel_pool.idle, &idx);
#ifdef ENABLE_CHECKING
  if (get_thread_state (&kernel_pool.threads[idx].t) == THREAD_BUSY)
    abort ();
#endif
  set_thread_state (&kernel_pool.threads[idx].t, THREAD_BUSY);
  kernel_pool.threads[idx].t.from_sid = this_node->id;
  kernel_pool.threads[idx].t.u.kernel.buf = buf;
  kernel_pool.threads[idx].t.u.kernel.buf_size = buf_size;
  kernel_pool.threads[idx].t.u.kernel.fuse_ch = ch;

  /* Let the thread run.  */
  semaphore_up (&kernel_pool.threads[idx].t.sem, 1);

  zfsd_mutex_unlock (&kernel_pool.mutex);
}

/*! Main function of the main (i.e. listening) kernel thread.  */

static void *
kernel_main (ATTRIBUTE_UNUSED void *data)
{
  size_t fuse_buf_size;

  thread_disable_signals ();
  pthread_setspecific (thread_name_key, "Kernel main thread");

  fuse_buf_size = fuse_chan_bufsize (fuse_ch);

  while (!thread_pool_terminate_p (&kernel_pool))
    {
      struct fuse_chan *ch_copy;
      int recv_res;

      zfsd_mutex_lock (&buffer_pool_mutex);
      if (buffer_pool_size == 0)
        {
	  buffer_pool[0] = xmalloc (fuse_buf_size);
          buffer_pool_size++;
        }
      zfsd_mutex_unlock (&buffer_pool_mutex);
      /* buffer_pool[0] is now available, and no other thread will remove it,
	 so it is safe to unlock the mutex. */

      ch_copy = fuse_ch;
      zfsd_mutex_lock (&kernel_pool.main_in_syscall);
      recv_res = fuse_chan_recv (&ch_copy, buffer_pool[0], fuse_buf_size);
      zfsd_mutex_unlock (&kernel_pool.main_in_syscall);

      if (thread_pool_terminate_p (&kernel_pool))
        {
          message (LOG_NOTICE, FACILITY_ZFSD | FACILITY_NET | FACILITY_THREADING, "Kernel thread terminating\n");
          break;
        }

      if (recv_res == -EINTR || recv_res == -EAGAIN)
	continue;

      if (recv_res <= 0)
        {
	  if (recv_res != ENODEV)
	    message (LOG_NOTICE, FACILITY_ZFSD | FACILITY_DATA, "FUSE unmounted, kernel_main exiting\n");
	  else
	    message (LOG_NOTICE, FACILITY_ZFSD | FACILITY_THREADING, "kernel_main exiting: %s\n",
		     strerror (-recv_res));
          break;
        }

      /* Dispatch the packet.  */
      kernel_dispatch (ch_copy, buffer_pool[0], recv_res);
      zfsd_mutex_lock (&buffer_pool_mutex);
      buffer_pool_size--;
      if (buffer_pool_size > 0)
	buffer_pool[0] = buffer_pool[buffer_pool_size];
      zfsd_mutex_unlock (&buffer_pool_mutex);
    }

  message (LOG_NOTICE, FACILITY_ZFSD | FACILITY_THREADING, "Kernel thread return...\n");
  return NULL;
}

/*! Open the FUSE mount and start the main kernel thread.  */
bool
kernel_start (void)
{
  fuse_ino_t root_ino;

  zfsd_mutex_init (&buffer_pool_mutex);

  if (fuse_parse_cmdline (&main_args, &fuse_mountpoint, NULL, NULL) != 0)
    exit (EXIT_FAILURE);

  inode_map_init ();

  root_ino = fh_to_inode (&root_fh);
  assert (root_ino == FUSE_ROOT_ID);

  fuse_ch = fuse_mount (fuse_mountpoint, &main_args);
  if (fuse_ch == NULL)
    goto err;
  fuse_se = fuse_lowlevel_new (&main_args, &zfs_fuse_ops, sizeof (zfs_fuse_ops),
			       NULL);
  if (fuse_se == NULL)
    goto err_ch;
  fuse_session_add_chan (fuse_se, fuse_ch);

  if (!thread_pool_create (&kernel_pool, &kernel_thread_limit, kernel_main,
                           kernel_worker, kernel_worker_init))
    {
      kernel_unmount ();
      return false;
    }

  return true;

 err_ch:
  fuse_unmount (fuse_mountpoint, fuse_ch);
 err:
  return false;
}

/*! Terminate kernel threads and destroy data structures.  */

void
kernel_cleanup (void)
{
  thread_pool_destroy (&kernel_pool);
}
