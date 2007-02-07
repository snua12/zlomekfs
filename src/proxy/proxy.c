#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse_lowlevel.h>

#include "fh.h"
#include "hashtab.h"
#include "log.h"
#include "proxy.h"
#include "zfs-prot.h"

/* Cast away constness while avoiding a gcc -Wcast-qual warning; TYPE must be
   "T *" and VALUE must have type "const T *".

   The subtraction is there to ensure this type relation; it is wrapped in
   sizeof to avoid evaluating VALUE twice - or even evaluating the subtraction,
   which has undefined value (assuming TYPE is not a variably-modified
   type). */
#define CONST_CAST(TYPE, VALUE) \
  ((void)sizeof ((VALUE) - (TYPE)0), (TYPE)(intptr_t)(VALUE))

/* In seconds before a revalidation is required */
#define CACHE_VALIDITY 5

#define ZFSD_SOCKET "/home/mitr/z/socket"

static int zfsd_fd;

static fuse_ino_t next_ino;

 /* Inode <-> file handle mapping */

struct inode_map
{
  fuse_ino_t ino;
  zfs_fh fh;
};

static htab_t inode_map_ino, inode_map_fh;

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

/* FIXME?
   This means that (find zfs_root) will permanently pin a lot of memory... */
static fuse_ino_t
fh_to_inode (const zfs_fh *fh)
{
  void **slot;
  struct inode_map *map;

  slot = htab_find_slot (inode_map_fh, &fh, INSERT);
  if (*slot != NULL)
    {
      map = *slot;
      return map->ino;
    }
  map = xmalloc (sizeof (*map));
  map->ino = next_ino;
  next_ino++;
  map->fh = *fh;
  *slot = map;
  slot = htab_find_slot (inode_map_ino, &map->ino, INSERT);
  assert (*slot == NULL);
  *slot = map;
  return map->ino;
}

static const zfs_fh *
inode_to_fh (fuse_ino_t ino)
{
  struct inode_map *map;

  map = htab_find (inode_map_ino, &ino);
  if (map != NULL)
    return &map->fh;
  else
    return NULL;
}

 /* Common infrastructure */

int
call_request (struct request *req)
{
  direction dir;
  uint32_t reply_id;

  if (!full_write (zfsd_fd, req->dc.buffer, req->dc.cur_length))
    return EIO;
  if (!full_read (zfsd_fd, req->dc.buffer, 4))
    return EIO;
  if (!start_decoding (&req->dc))
    {
      message (1, stderr, "Invalid reply length %" PRIu32 "\n",
	       req->dc.max_length);
      return EPROTO;
    }
  if (!full_read (zfsd_fd, req->dc.buffer + 4, req->dc.max_length - 4))
    return EIO;
  /* FIXME: handle asynchronous cache invalidation */
  if (!decode_direction (&req->dc, &dir) || dir != DIR_REPLY
      || !decode_request_id (&req->dc, &reply_id))
    {
      message (1, stderr, "Invalid reply\n");
      return EPROTO;
    }
  if (reply_id != req->id)
    {
      message (1, stderr,
	       "Reply ID does not match: req %" PRIu32 ", rep %" PRIu32 "\n",
	       req->id, reply_id);
      return EPROTO;
    }
  return 0;
}

static ftype ftype_from_mode_t (mode_t mode)
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
  attr->uid = ctx->uid;
  attr->gid = ctx->gid;
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
  st->st_uid = fa->uid;
  st->st_gid = fa->gid;
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
zfs_proxy_lookup (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  dir_op_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  dc_init (&rq.dc);
  err = zfs_call_lookup (&rq, &args);
  free (args.name.str);
  if (err != 0)
    /* FIXME: return fuse_entry_param with ino = 0 to create a negative
       dentry? */
    goto err;
  if (!decode_dir_op_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_lookup reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_getattr (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  struct stat st;
  struct request rq;
  const zfs_fh *fh;
  fattr fa;
  int err;

  (void)fi;
  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  dc_init (&rq.dc);
  err = zfs_call_getattr (&rq, fh);
  if (err != 0)
    goto err;
  if (!decode_fattr (&rq.dc, &fa) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_getattr reply\n");
      err = EPROTO;
      goto err;
    }
  stat_from_fattr (&st, &fa, ino);
  fuse_reply_attr (req, &st, CACHE_VALIDITY);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_setattr (fuse_req_t req, fuse_ino_t ino, struct stat *attr,
		   int to_set, struct fuse_file_info *fi)
{
  struct stat st;
  struct request rq;
  const zfs_fh *fh;
  setattr_args args;
  fattr fa;
  int err;

  (void)fi;
  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.file = *fh;
  if ((to_set & FUSE_SET_ATTR_MODE) != 0)
    args.attr.mode = attr->st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU
				      | S_IRWXG | S_IRWXO);
  else
    args.attr.mode = -1;
  if ((to_set & FUSE_SET_ATTR_UID) != 0)
    args.attr.uid = attr->st_uid;
  else
    args.attr.uid = -1;
  if ((to_set & FUSE_SET_ATTR_GID) != 0)
    args.attr.gid = attr->st_gid;
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
  dc_init (&rq.dc);
  err = zfs_call_setattr (&rq, &args);
  if (err != 0)
    goto err;
  if (!decode_fattr (&rq.dc, &fa) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_setattr reply\n");
      err = EPROTO;
      goto err;
    }
  stat_from_fattr (&st, &fa, ino);
  fuse_reply_attr (req, &st, CACHE_VALIDITY);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_readlink (fuse_req_t req, fuse_ino_t ino)
{
  struct request rq;
  const zfs_fh *fh;
  string path;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  dc_init (&rq.dc);
  err = zfs_call_readlink (&rq, fh);
  if (err != 0)
    goto err;
  if (!decode_zfs_path (&rq.dc, &path) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_readlink reply\n");
      err = EPROTO;
      goto err;
    }
  fuse_reply_readlink (req, path.str);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_mknod (fuse_req_t req, fuse_ino_t parent, const char *name,
		 mode_t mode, dev_t rdev)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  mknod_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
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
      message (1, stderr, "Invalid file type in mknod\n");
      free (args.where.name.str);
      err = EINVAL;
      goto err;
    }
  args.rdev = rdev;
  dc_init (&rq.dc);
  err = zfs_call_mknod (&rq, &args);
  free (args.where.name.str);
  if (err != 0)
    goto err;
  if (!decode_dir_op_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_mknod reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_mkdir (fuse_req_t req, fuse_ino_t parent, const char *name,
		 mode_t mode)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  mkdir_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.where.dir = *fh;
  xmkstring (&args.where.name, name);
  sattr_from_req (&args.attr, req);
  args.attr.mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG
			   | S_IRWXO);
  dc_init (&rq.dc);
  err = zfs_call_mkdir (&rq, &args);
  free (args.where.name.str);
  if (err != 0)
    goto err;
  if (!decode_dir_op_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_mkdir reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_unlink (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct request rq;
  const zfs_fh *fh;
  dir_op_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  dc_init (&rq.dc);
  err = zfs_call_unlink (&rq, &args);
  free (args.name.str);
  if (err != 0)
    goto err;
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_unlink reply\n");
      err = EPROTO;
      goto err;
    }
  /* Fall through */
 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_rmdir (fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct request rq;
  const zfs_fh *fh;
  dir_op_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.dir = *fh;
  xmkstring (&args.name, name);
  dc_init (&rq.dc);
  err = zfs_call_rmdir (&rq, &args);
  free (args.name.str);
  if (err != 0)
    goto err;
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_rmdir reply\n");
      err = EPROTO;
      goto err;
    }
  /* Fall through */
 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_symlink (fuse_req_t req, const char *dest, fuse_ino_t parent,
		   const char *name)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  symlink_args args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.from.dir = *fh;
  xmkstring (&args.from.name, name);
  xmkstring (&args.to, dest);
  sattr_from_req (&args.attr, req);
  dc_init (&rq.dc);
  err = zfs_call_symlink (&rq, &args);
  free (args.from.name.str);
  free (args.to.str);
  if (err != 0)
    goto err;
  if (!decode_dir_op_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_symlink reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_rename (fuse_req_t req, fuse_ino_t parent, const char *name,
		  fuse_ino_t newparent, const char *newname)
{
  struct request rq;
  const zfs_fh *fh;
  rename_args args;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.from.dir = *fh;
  fh = inode_to_fh (newparent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.to.dir = *fh;
  xmkstring (&args.from.name, name);
  xmkstring (&args.to.name, newname);
  dc_init (&rq.dc);
  err = zfs_call_rename (&rq, &args);
  free (args.from.name.str);
  free (args.to.name.str);
  if (err != 0)
    goto err;
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_rename reply\n");
      err = EPROTO;
      goto err;
    }
  /* Fall through */
 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_link (fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
		const char *newname)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  link_args args;
  dir_op_args lookup_args;
  dir_op_res res;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.from = *fh;
  fh = inode_to_fh (newparent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.to.dir = *fh;
  xmkstring (&args.to.name, newname);
  dc_init (&rq.dc);
  err = zfs_call_link (&rq, &args);
  free (args.to.name.str);
  if (err != 0)
    goto err;
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_rename reply\n");
      err = EPROTO;
      goto err;
    }
  lookup_args.dir = *fh;
  xmkstring (&lookup_args.name, newname);
  dc_init (&rq.dc);
  err = zfs_call_lookup (&rq, &lookup_args);
  free (lookup_args.name.str);
  if (err != 0)
    goto err;
  if (!decode_dir_op_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_lookup reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res);
  fuse_reply_entry (req, &e);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_open (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  struct request rq;
  const zfs_fh *fh;
  open_args args;
  zfs_cap res, *cap;
  int err;

  fh = inode_to_fh (ino);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.file = *fh;
  args.flags = fi->flags;
  dc_init (&rq.dc);
  err = zfs_call_open (&rq, &args);
  if (err != 0)
    goto err;
  if (!decode_zfs_cap (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_open reply\n");
      err = EPROTO;
      goto err;
    }
  cap = xmalloc (sizeof (*cap));
  *cap = res;
  fi->fh = (intptr_t)cap;
  fi->direct_io = 0; /* Use the page cache */
  fi->keep_cache = 1;
  fuse_reply_open (req, fi);
  /* FIXME: release the file if reply_open fails? */
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_read (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		struct fuse_file_info *fi)
{
  struct request rq;
  read_args args;
  read_res res;
  zfs_cap *cap;
  int err;

  (void)ino;
  cap = (zfs_cap *)(intptr_t)fi->fh;
  args.cap = *cap;
  args.offset = off;
  args.count = size;
  dc_init (&rq.dc);
  /* FIXME: handle size > ZFS_MAXDATA? */
  err = zfs_call_read (&rq, &args);
  if (err != 0)
    goto err;
  if (!decode_read_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_read reply\n");
      err = EPROTO;
      goto err;
    }
  fuse_reply_buf (req, res.data.buf, res.data.len);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_write (fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size,
		 off_t off, struct fuse_file_info *fi)
{
  struct request rq;
  write_args args;
  write_res res;
  zfs_cap *cap;
  int err;

  (void)ino;
  cap = (zfs_cap *)(intptr_t)fi->fh;
  args.cap = *cap;
  args.offset = off;
  args.data.len = size;
  args.data.buf = CONST_CAST (char *, buf);
  dc_init (&rq.dc);
  /* FIXME: handle size > ZFS_MAXDATA? */
  err = zfs_call_write (&rq, &args);
  if (err != 0)
    goto err;
  if (!decode_write_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_write reply\n");
      err = EPROTO;
      goto err;
    }
  fuse_reply_write (req, res.written);
  return;

 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_release (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
  /* FIXME FIXME: write out any pending modifications in the page cache and
     invalidate the cache */
  struct request rq;
  zfs_cap *cap;
  int err;

  (void)ino;
  cap = (zfs_cap *)(intptr_t)fi->fh;
  dc_init (&rq.dc);
  /* FIXME: handle size > ZFS_MAXDATA? */
  err = zfs_call_close (&rq, cap);
  free (cap);
  if (err != 0)
    goto err;
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_close reply\n");
      err = EPROTO;
      goto err;
    }
  /* Fall through */
 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_readdir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		   struct fuse_file_info *fi)
{
  struct request rq;
  read_dir_args args;
  dir_list list;
  zfs_cap *cap;
  uint32_t i;
  int err;
  char *buf;
  size_t buf_size, buf_offset;

  (void)ino;
  cap = (zfs_cap *)(intptr_t)fi->fh;
  args.cap = *cap;
  args.cookie = off;
  args.count = size < ZFS_MAXDATA ? size : ZFS_MAXDATA;
  dc_init (&rq.dc);
  err = zfs_call_readdir (&rq, &args);
  if (err != 0)
    goto err;
  if (!decode_dir_list (&rq.dc, &list) || list.n > ZFS_MAX_DIR_ENTRIES)
    {
      message (1, stderr, "Invalid zfs_readdir reply\n");
      err = EPROTO;
      goto err;
    }
  buf_size = args.count;
  buf = xmalloc (buf_size);
  buf_offset = 0;
  for (i = 0; i < list.n; i++)
    {
      dir_entry entry;
      struct stat st;
      size_t sz;

      if (!decode_dir_entry (&rq.dc, &entry))
	{
	  message (1, stderr, "Invalid zfs_readdir reply\n");
	  err = EPROTO;
	  goto err_buf;
	}
      st.st_ino = entry.ino;
      st.st_mode = 0;
      sz = fuse_add_direntry (req, buf + buf_offset, buf_size - buf_offset,
			      entry.name.str, &st, entry.cookie);
      if (buf_offset + sz > buf_size)
	{
	  do
	    buf_size *= 2;
	  while (buf_offset + sz > buf_size);
	  buf = xrealloc (buf, buf_size);
	  sz = fuse_add_direntry (req, buf + buf_offset, buf_size - buf_offset,
				  entry.name.str, &st, entry.cookie);
	  assert (buf_offset + sz <= buf_size);
	}
      buf_offset += sz;
    }
  if (!finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_close reply\n");
      err = EPROTO;
      goto err_buf;
    }
  fuse_reply_buf (req, buf, buf_offset);
  free (buf);
  return;

 err_buf:
  free (buf);
 err:
  fuse_reply_err (req, err);
}

static void
zfs_proxy_statfs (fuse_req_t req, fuse_ino_t ino)
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
zfs_proxy_create (fuse_req_t req, fuse_ino_t parent, const char *name,
		  mode_t mode, struct fuse_file_info *fi)
{
  struct fuse_entry_param e;
  struct request rq;
  const zfs_fh *fh;
  create_args args;
  create_res res;
  zfs_cap *cap;
  int err;

  fh = inode_to_fh (parent);
  if (fh == NULL)
    {
      err = EBADF; /* FIXME? other value? */
      goto err;
    }
  args.where.dir = *fh;
  xmkstring (&args.where.name, name);
  args.flags = fi->flags;
  sattr_from_req (&args.attr, req);
  args.attr.mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG
			   | S_IRWXO);
  dc_init (&rq.dc);
  err = zfs_call_create (&rq, &args);
  free (args.where.name.str);
  if (err != 0)
    goto err;
  if (!decode_create_res (&rq.dc, &res) || !finish_decoding (&rq.dc))
    {
      message (1, stderr, "Invalid zfs_open reply\n");
      err = EPROTO;
      goto err;
    }
  entry_from_dir_op_res (&e, &res.dor);
  cap = xmalloc (sizeof (*cap));
  *cap = res.cap;
  fi->fh = (intptr_t)cap;
  fi->direct_io = 0; /* Use the page cache */
  fi->keep_cache = 1;
  fuse_reply_create (req, &e, fi);
  /* FIXME: release the file if reply_create fails? */
  return;

 err:
  fuse_reply_err (req, err);
}

static struct fuse_lowlevel_ops zfs_proxy_ops =
  {
    .lookup = zfs_proxy_lookup,
    /* FIXME? .forget */
    .getattr = zfs_proxy_getattr,
    .setattr = zfs_proxy_setattr,
    .readlink = zfs_proxy_readlink,
    .mknod = zfs_proxy_mknod,
    .mkdir = zfs_proxy_mkdir,
    .unlink = zfs_proxy_unlink,
    .rmdir = zfs_proxy_rmdir,
    .symlink = zfs_proxy_symlink,
    .rename = zfs_proxy_rename,
    .link = zfs_proxy_link,
    .open = zfs_proxy_open,
    .read = zfs_proxy_read,
    .write = zfs_proxy_write,
    /* .flush not necessary */
    .release = zfs_proxy_release,
    /* .fsync not implemented */
    .opendir = zfs_proxy_open,
    .readdir = zfs_proxy_readdir,
    .releasedir = zfs_proxy_release,
    /* .fsyncdir not implemented */
    .statfs = zfs_proxy_statfs,
    /* .setxattr, .getxattr, .listxattr, .removexattr not implemented */
    /* .access not necessary, the filesystem should be mounted with
       -o default_permissions */
    .create = zfs_proxy_create,
    /* .getlk, .setlk not implemented */
    /* .bmap not applicable */
  };

static int
connect_to_zfsd (void)
{
  struct sockaddr_un sun;
  int fd;

  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1)
    {
      message (-1, stderr, "Error creating a socket: %s\n", strerror (errno));
      goto err;
    }
  (void)unlink (ZFSD_SOCKET);
  sun.sun_family = AF_UNIX;
  assert (sizeof (sun.sun_path) >= sizeof (ZFSD_SOCKET));
  strcpy (sun.sun_path, ZFSD_SOCKET);
  if (bind (fd, (struct sockaddr *)&sun, sizeof (sun)) != 0)
    {
      message (-1, stderr, "Error binding a socket to %s: %s", ZFSD_SOCKET,
	       strerror (errno));
      goto err_fd;
    }
  if (listen (fd, 1) != 0)
    {
      message (-1, stderr, "Error listening on %s: %s", ZFSD_SOCKET,
	       strerror (errno));
      goto err_socket;
    }
  zfsd_fd = accept (fd, NULL, NULL);
  if (zfsd_fd == -1)
    {
      message (-1, stderr, "Error accepting a connection on %s: %s",
	       ZFSD_SOCKET, strerror (errno));
      goto err_socket;
    }
  close (fd);
  unlink (ZFSD_SOCKET);
  return 0;

 err_socket:
  unlink (ZFSD_SOCKET);
 err_fd:
  close (fd);
 err:
  return -1;
}

int
main (int argc, char *argv[])
{
  struct fuse_args args;
  char *mountpoint;
  struct fuse_session *se;
  struct fuse_chan *ch;
  struct request req;
  zfs_fh root_fh;
  fuse_ino_t root_ino;
  int res;

  args = (const struct fuse_args) FUSE_ARGS_INIT (argc, argv);
  res = EXIT_FAILURE;
  if (fuse_parse_cmdline (&args, &mountpoint, NULL, NULL) != 0)
    goto err;

  if (connect_to_zfsd () != 0)
    goto err_args;

  inode_map_ino = htab_create (100, inode_map_ino_hash, inode_map_ino_eq, NULL,
			       NULL);
  inode_map_fh = htab_create (100, inode_map_fh_hash, inode_map_fh_eq, NULL,
			      NULL);
  dc_init (&req.dc);
  if (zfs_call_root (&req, NULL) != 0)
    goto err_zfsd;
  /* FIXME: integrate to z-p.c. */
  if (!decode_zfs_fh (&req.dc, &root_fh) || !finish_decoding (&req.dc))
    {
      message (1, stderr, "Invalid zfs_proc_root reply\n");
      goto err_zfsd;
    }
  next_ino = FUSE_ROOT_ID;
  root_ino = fh_to_inode (&root_fh);
  assert (root_ino == FUSE_ROOT_ID);

  ch = fuse_mount (mountpoint, &args);
  if (ch == NULL)
    goto err_args;
  se = fuse_lowlevel_new (&args, &zfs_proxy_ops, sizeof (zfs_proxy_ops), NULL);
  if (se == NULL)
    goto err_ch;
  if (fuse_set_signal_handlers (se) != 0)
    goto err_se;
  fuse_session_add_chan (se, ch);
  if (fuse_session_loop (se) != 0)
    goto err_chan;
  res = EXIT_SUCCESS;

 err_chan:
  fuse_session_remove_chan (ch);
 err_se:
  fuse_session_destroy (se);
 err_ch:
  fuse_unmount (mountpoint, ch);
 err_zfsd:
  close (zfsd_fd);
 err_args:
  fuse_opt_free_args (&args);
 err:
  return res;
}
