/* Functions for updating files.
   Copyright (C) 2003, 2004 Josef Zlomek

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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "pthread.h"
#include "constant.h"
#include "update.h"
#include "md5.h"
#include "memory.h"
#include "alloc-pool.h"
#include "queue.h"
#include "log.h"
#include "volume.h"
#include "fh.h"
#include "cap.h"
#include "varray.h"
#include "interval.h"
#include "zfs_prot.h"
#include "file.h"
#include "dir.h"

/* Queue of file handles.  */
queue update_queue;

/* Pool of update threads.  */
thread_pool update_pool;

/* Get blocks of file FH from interval [START, END) which need to be updated
   and store them to BLOCKS.  */

void
get_blocks_for_updating (internal_fh fh, uint64_t start, uint64_t end,
			 varray *blocks)
{
  varray tmp;

  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (!fh->updated)
    abort ();
  if (!fh->modified)
    abort ();
#endif

  interval_tree_complement (fh->updated, start, end, &tmp);
  interval_tree_complement_varray (fh->updated, &tmp, blocks);
  varray_destroy (&tmp);
}

/* Update some of the BLOCKS (described in ARGS) of local file CAP
   from remote file.  If USE_BUFFER is true, load the needed intervals
   to BUFFER (the beginning of buffer is at file offset OFFSET) and
   store the updated number of bytes read from file to RCOUNT.  */

static int32_t
update_file_blocks_1 (bool use_buffer, uint32_t *rcount, void *buffer,
		      uint64_t offset, md5sum_args *args, zfs_cap *cap,
		      varray *blocks)
{
  volume vol;
  internal_dentry dentry;
  md5sum_res local_md5;
  md5sum_res remote_md5;
  int32_t r;
  unsigned int i, j;

#ifdef ENABLE_CHECKING
  if (VIRTUAL_FH_P (cap->fh))
    abort ();
#endif

  args->cap = *cap;
  r = remote_md5sum (&remote_md5, args);
  if (r != ZFS_OK)
    return r;

  if (remote_md5.count == 0)
    return ZFS_OK;

  args->cap = *cap;
  r = local_md5sum (&local_md5, args);
  if (r != ZFS_OK)
    return r;

  r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return r;

#ifdef ENABLE_CHECKING
  if (!(vol->local_path && vol->master != this_node))
    abort ();
#endif

  /* If remote file is shorter truncate local file.  */
  if (local_md5.count > remote_md5.count
      || ((local_md5.offset[local_md5.count - 1]
	   + local_md5.length[local_md5.count - 1])
	  < (remote_md5.offset[remote_md5.count - 1]
	     + remote_md5.length[remote_md5.count - 1])))
    {
      sattr sa;

      memset (&sa, -1, sizeof (sattr));
      sa.size = (remote_md5.offset[remote_md5.count - 1]
		 + remote_md5.length[remote_md5.count - 1]);

      r = local_setattr (&dentry->fh->attr, dentry, &sa, vol);
      if (r != ZFS_OK)
	return r;

      local_md5.count = remote_md5.count;
    }
  else
    zfsd_mutex_unlock (&vol->mutex);

  /* DENTRY->FH->MUTEX is still locked.  */

  /* Delete the same blocks from MODIFIED interval tree and add them to
     UPDATED interval tree.  */
  for (i = 0; i < local_md5.count; i++)
    {
      if (local_md5.offset[i] != remote_md5.offset[i])
	{
	  abort (); /* FIXME: return some error */
	  return ZFS_UPDATE_FAILED;
	}

      if (local_md5.length[i] == remote_md5.length[i]
	  && memcmp (local_md5.md5sum[i], remote_md5.md5sum[i], MD5_SIZE) == 0)
	{
	  interval_tree_delete (dentry->fh->modified, local_md5.offset[i],
				local_md5.offset[i] + local_md5.length[i]);
	  interval_tree_insert (dentry->fh->updated, local_md5.offset[i],
				local_md5.offset[i] + local_md5.length[i]);
	}
    }
  release_dentry (dentry);

  /* Update different blocks.  */
  for (i = 0, j = 0; i < remote_md5.count; i++)
    {
      if (i >= local_md5.count
	  || local_md5.length[i] != remote_md5.length[i]
	  || memcmp (local_md5.md5sum[i], remote_md5.md5sum[i], MD5_SIZE) != 0)
	{
	  uint32_t count;
	  data_buffer data;
	  char tmp_buf[ZFS_MAXDATA];
	  char *buf;

	  while (j < VARRAY_USED (*blocks)
		 && (VARRAY_ACCESS (*blocks, j, interval).end
		     < remote_md5.offset[i]))
	    j++;

	  if ((VARRAY_ACCESS (*blocks, j, interval).start
	       <= remote_md5.offset[i])
	      && (remote_md5.offset[i] + remote_md5.length[i]
		  <= VARRAY_ACCESS (*blocks, j, interval).end))
	    {
	      /* MD5 block is not larger than the block to be updated.  */

	      if (use_buffer)
		buf = (char *) buffer + remote_md5.offset[i] - offset;
	      else
		buf = tmp_buf;

	      r = full_remote_read (&remote_md5.length[i], buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      r = full_local_write (&count, buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;
	      if (count != remote_md5.length[i])
		abort ();	/* FIXME */
	    }
	  else
	    {
	      /* MD5 block is larger than block(s) to be updated.  */

	      if (use_buffer)
		buf = (char *) buffer + remote_md5.offset[i] - offset;
	      else
		{
		  buf = data.real_buffer;

		  r = full_local_read (&count, buf, cap,
				       remote_md5.offset[i],
				       remote_md5.length[i]);
		  if (r != ZFS_OK)
		    return r;
		  if (count != remote_md5.length[i])
		    abort (); /* FIXME */
		}

	      r = full_remote_read (&remote_md5.length[i], tmp_buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      /* Update the blocks in buffer BUF.  */
	      for (; (j < VARRAY_USED (*blocks)
		      && (VARRAY_ACCESS (*blocks, j, interval).end
			  < remote_md5.offset[i])); j++)
		{
		  uint64_t start;
		  uint64_t end;

		  start = VARRAY_ACCESS (*blocks, j, interval).start;
		  if (start < remote_md5.offset[i])
		    start = remote_md5.offset[i];
		  end = VARRAY_ACCESS (*blocks, j, interval).end;
		  if (end > remote_md5.offset[i] + remote_md5.length[i])
		    end = remote_md5.offset[i] + remote_md5.length[i];

		  memcpy (buf + start - remote_md5.offset[i],
			  tmp_buf + start - remote_md5.offset[i],
			  end - start);
		}

	      /* Write updated buffer BUF.  */
	      r = full_local_write (&count, buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;
	      if (count != remote_md5.length[i])
		abort ();	/* FIXME */

	      /* Add the interval to UPDATED.  */
	      r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL);
	      if (r != ZFS_OK)
		return r;

	      interval_tree_insert (dentry->fh->updated, remote_md5.offset[i],
				    (remote_md5.offset[i]
				     + remote_md5.length[i]));
	      release_dentry (dentry);
	      zfsd_mutex_unlock (&vol->mutex);
	    }
	}
    }

  if (use_buffer)
    {
      *rcount = (remote_md5.offset[remote_md5.count - 1]
		 + remote_md5.length[remote_md5.count - 1]) - offset;
    }

  return ZFS_OK;
}

/* Update BLOCKS of file CAP.  If USE_BUFFER is true, load the needed intervals
   to BUFFER (the beginning of buffer is at file offset OFFSET) and store the
   updated number of bytes read from file to RCOUNT.  */

int32_t
update_file_blocks (bool use_buffer, uint32_t *rcount, void *buffer,
		    uint64_t offset, zfs_cap *cap, varray *blocks)
{
  md5sum_args args;
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  int32_t r, r2;
  unsigned int i;

#ifdef ENABLE_CHECKING
  if (VARRAY_USED (*blocks) == 0)
    abort ();
#endif

  r2 = find_capability (cap, &icap, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (zfs_cap_undefined (icap->master_cap))
    {
      zfs_cap master_cap;

      r = remote_open (&master_cap, icap, O_RDWR, dentry, vol);
      if (r != ZFS_OK)
	return r;

      r2 = find_capability (cap, &icap, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      icap->master_cap = master_cap;
    }

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  args.count = 0;
  for (i = 0; i < VARRAY_USED (*blocks); i++)
    {
      interval x;

      x = VARRAY_ACCESS (*blocks, i, interval);
      do
	{
	  if (args.count > 0
	      && (x.start - args.offset[args.count - 1] < ZFS_MAXDATA)
	      && (x.start - args.offset[args.count - 1]
		  - args.length[args.count - 1] < ZFS_MODIFIED_BLOCK_SIZE))
	    {
	      x.start = args.offset[args.count - 1];
	      args.length[args.count - 1] = (x.end - x.start < ZFS_MAXDATA
					     ? x.end - x.start : ZFS_MAXDATA);
	      x.start += args.length[args.count];
	    }
	  else
	    {
	      if (args.count == ZFS_MAX_MD5_CHUNKS)
		{
		  r = update_file_blocks_1 (use_buffer, rcount, buffer, offset,
					    &args, cap, blocks);
		  if (r != ZFS_OK)
		    return r;
		  args.count = 0;
		}
	      args.offset[args.count] = x.start;
	      args.length[args.count] = (x.end - x.start < ZFS_MAXDATA
					 ? x.end - x.start : ZFS_MAXDATA);
	      x.start += args.length[args.count];
	      args.count++;
	    }
	}
      while (x.start < x.end);
    }

  if (args.count > 0)
    {
      r = update_file_blocks_1 (use_buffer, rcount, buffer, offset, &args,
				cap, blocks);
      if (r != ZFS_OK)
	return r;
    }

  r2 = find_capability (cap, &icap, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (dentry->fh->meta.flags & METADATA_COMPLETE)
    {
      r = remote_close (icap, dentry, vol);
      if (r != ZFS_OK)
	return r;
    }
  else
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
    }

  return ZFS_OK;
}

/* Return true if file *DENTRYP on volume *VOLP with file handle FH should
   be updated.  Store remote attributes to ATTR.  */

bool
update_p (internal_dentry *dentryp, volume *volp, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;

  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!((*volp)->local_path && (*volp)->master != this_node))
    abort ();
#endif

  release_dentry (*dentryp);
  zfsd_mutex_unlock (&(*volp)->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = refresh_master_fh (fh);
  if (r != ZFS_OK)
    goto out;

  r2 = zfs_fh_lookup (fh, volp, dentryp, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  r = remote_getattr (attr, *dentryp, *volp);
  if (r != ZFS_OK)
    goto out;

  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* We would destroy locked dentry which would cause problems in
     UPDATE_FH_IF_NEEDED* macros it we returned true.  */
  if ((*dentryp)->fh->attr.type == FT_DIR && attr->type != FT_DIR)
    return false;

  return UPDATE_P (*dentryp, *attr);

out:
  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  return false;
}

/* Delete file in place of file DENTRY on volume VOL.  */

static bool
delete_tree (internal_dentry dentry, volume vol)
{
  char *path;
  uint32_t vid;
  bool r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  path = build_local_path (vol, dentry);
  vid = vol->id;
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = recursive_unlink (path, vid);
  free (path);

  return r;
}

/* Delete file NAME in directory DIR on volume VOL.  */

static bool
delete_tree_name (internal_dentry dir, char *name, volume vol)
{
  char *path;
  uint32_t vid;
  bool r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name);
  vid = vol->id;
  release_dentry (dir);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&vol->mutex);
  r = recursive_unlink (path, vid);
  free (path);

  return r;
}

/* Update generic file DENTRY on volume VOL with name NAME, with local file
   handle LOCAL_FH, remote file handle REMOTE_FH and remote attributes
   REMOTE_ATTR.  DIR_FH is a file handle of the directory.
   Store updated local file handle to LOCAL_FH.  */

static int32_t
update_local_fh (internal_dentry dentry, string *name, volume vol,
		 zfs_fh *dir_fh, zfs_fh *local_fh, zfs_fh *remote_fh,
		 fattr *remote_attr)
{
  internal_dentry dir;
  fattr *local_attr;
  sattr sa;
  int32_t r, r2;
  read_link_res link_to;
  create_res cr_res;
  dir_op_res res;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (remote_attr->type != FT_DIR && !dentry->parent)
    abort ();
#endif

  local_attr = &res.attr;
  switch (remote_attr->type)
    {
      default:
	abort ();

      case FT_BAD:
	if (!delete_tree (dentry, vol))
	  return ZFS_UPDATE_FAILED;
	break;

      case FT_REG:
	if (remote_attr->type != dentry->fh->attr.type)
	  {
	    int fd;

	    if (!delete_tree (dentry, vol))
	      return ZFS_UPDATE_FAILED;

	    r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
	    if (r2 != ZFS_OK)
	      abort ();
#endif

	    sa.mode = remote_attr->mode;
	    sa.uid = remote_attr->uid;
	    sa.gid = remote_attr->gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_create (&cr_res, &fd, dir, name,
			      O_CREAT | O_WRONLY | O_TRUNC, &sa, vol);
	    if (r == ZFS_OK)
	      {
		close (fd);
		*local_fh = cr_res.file;
		local_attr = &cr_res.attr;
	      }
	  }
	else
	  {
	    sa.mode = remote_attr->mode;
	    sa.uid = remote_attr->uid;
	    sa.gid = remote_attr->gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_setattr (local_attr, dentry, &sa, vol);
	  }
	break;

      case FT_DIR:
	if (remote_attr->type != dentry->fh->attr.type)
	  {

#ifdef ENABLE_CHECKING
	    if (!dentry->parent)
	      abort ();
#endif

	    if (!delete_tree (dentry, vol))
	      return ZFS_UPDATE_FAILED;

	    r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
	    if (r2 != ZFS_OK)
	      abort ();
#endif

	    sa.mode = remote_attr->mode;
	    sa.uid = remote_attr->uid;
	    sa.gid = remote_attr->gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_mkdir (&res, dir, name, &sa, vol);
	    if (r == ZFS_OK)
	      {
		*local_fh = res.file;
	      }
	  }
	else
	  {
	    sa.mode = remote_attr->mode;
	    sa.uid = remote_attr->uid;
	    sa.gid = remote_attr->gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_setattr (local_attr, dentry, &sa, vol);
	  }
	break;

      case FT_LNK:
	zfsd_mutex_unlock (&fh_mutex);
	r = remote_readlink (&link_to, dentry, vol);
	if (r != ZFS_OK)
	  return r;

	r2 = zfs_fh_lookup_nolock (local_fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	if (!delete_tree (dentry, vol))
	  return ZFS_UPDATE_FAILED;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	sa.mode = (uint32_t) -1;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_symlink (&res, dir, name, &link_to.path, &sa, vol);
	if (r == ZFS_OK)
	  {
	    *local_fh = res.file;
	  }
	break;

      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	if (!delete_tree (dentry, vol))
	  return ZFS_UPDATE_FAILED;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	sa.mode = remote_attr->mode;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_mknod (&res, dir, name, &sa, remote_attr->type,
			 remote_attr->rdev, vol);
	if (r == ZFS_OK)
	  {
	    *local_fh = res.file;
	  }
	break;
    }

  if (r == ZFS_OK)
    {
      uint32_t flags;
      bool ok;

      if (dir_fh)
	{
	  r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();
	}
      else
	{
	  dir = NULL;
	  zfsd_mutex_lock (&fh_mutex);
	}

      dentry = get_dentry (local_fh, remote_fh, vol, dir, name->str,
			   local_attr);
      if (dir)
	release_dentry (dir);
      zfsd_mutex_unlock (&fh_mutex);

      if (dentry->fh->attr.type == FT_REG)
	flags = dentry->fh->meta.flags & METADATA_MODIFIED;
      else if (dentry->fh->attr.type == FT_DIR)
	flags = 0;
      else
	flags = METADATA_COMPLETE;

      ok = set_metadata (vol, dentry->fh, flags,
			 remote_attr->version, remote_attr->version);
      release_dentry (dentry);
      if (!ok)
	{
	  vol->flags |= VOLUME_DELETE;
	  r = ZFS_METADATA_ERROR;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }

  return r;
}

/* Create generic file NAME in directory DIR on volume VOL with remote file
   REMOTE_FH and remote attributes REMOTE_ATTR.  DIR_FH is a file handle of
   the directory.
   Store created local file handle to LOCAL_FH.  */

static int32_t
create_local_fh (internal_dentry dir, string *name, volume vol,
		 zfs_fh *dir_fh, zfs_fh *local_fh, zfs_fh *remote_fh,
		 fattr *remote_attr)
{
  internal_dentry dentry;
  fattr *local_attr;
  sattr sa;
  int32_t r, r2;
  read_link_res link_to;
  create_res cr_res;
  dir_op_res res;
  int fd;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  local_attr = &res.attr;
  switch (remote_attr->type)
    {
      default:
	abort ();

      case FT_BAD:
	release_dentry (dir);
	zfsd_mutex_unlock (&vol->mutex);
	zfsd_mutex_unlock (&fh_mutex);
	break;

      case FT_REG:
	sa.mode = remote_attr->mode;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_create (&cr_res, &fd, dir, name,
			  O_CREAT | O_WRONLY | O_TRUNC, &sa, vol);
	if (r == ZFS_OK)
	  {
	    close (fd);
	    *local_fh = cr_res.file;
	    local_attr = &cr_res.attr;
	  }
	break;

      case FT_DIR:
	sa.mode = remote_attr->mode;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_mkdir (&res, dir, name, &sa, vol);
	if (r == ZFS_OK)
	  {
	    *local_fh = res.file;
	  }
	break;

      case FT_LNK:
	release_dentry (dir);
	zfsd_mutex_unlock (&fh_mutex);

	r = remote_readlink_zfs_fh (&link_to, remote_fh, vol);
	if (r != ZFS_OK)
	  return r;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	sa.mode = (uint32_t) -1;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_symlink (&res, dir, name, &link_to.path, &sa, vol);
	if (r == ZFS_OK)
	  {
	    *local_fh = res.file;
	  }
	break;

      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	sa.mode = remote_attr->mode;
	sa.uid = remote_attr->uid;
	sa.gid = remote_attr->gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	r = local_mknod (&res, dir, name, &sa, remote_attr->type,
			 remote_attr->rdev, vol);
	if (r == ZFS_OK)
	  {
	    *local_fh = res.file;
	  }
	break;
    }

  if (r == ZFS_OK)
    {
      uint32_t flags;
      bool ok;

      r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      dentry = get_dentry (local_fh, remote_fh, vol, dir, name->str,
			   local_attr);
      release_dentry (dir);
      zfsd_mutex_unlock (&fh_mutex);

      if (dentry->fh->attr.type == FT_REG)
	flags = dentry->fh->meta.flags & METADATA_MODIFIED;
      else if (dentry->fh->attr.type == FT_DIR)
	flags = 0;
      else
	flags = METADATA_COMPLETE;

      ok = set_metadata (vol, dentry->fh, flags,
			 remote_attr->version, remote_attr->version);
      release_dentry (dentry);
      if (!ok)
	{
	  vol->flags |= VOLUME_DELETE;
	  r = ZFS_METADATA_ERROR;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }

  return r;
}

/* Update the directory *DIRP on volume *VOLP with file handle FH,
   set attributes according to ATTR.  */

int32_t
update_fh (internal_dentry dir, volume vol, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;
  internal_dentry dentry;
  filldir_htab_entries local_entries, remote_entries;
  dir_op_res local_res, remote_res;
  dir_entry *entry;
  void **slot, **slot2;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(vol->local_path && vol->master != this_node))
    abort ();
  if (attr->type != dir->fh->attr.type && !dir->parent)
    abort ();
#endif

  if (dir->parent)
    {
      zfs_fh parent_fh;
      string name;
      zfs_fh remote_fh;

      release_dentry (dir);
      zfsd_mutex_unlock (&vol->mutex);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
      if (!dentry->parent)
	abort ();
#endif
      zfsd_mutex_lock (&dentry->parent->fh->mutex);
      parent_fh = dir->parent->fh->local_fh;
      zfsd_mutex_unlock (&dentry->parent->fh->mutex);

      xmkstring (&name, dentry->name);
      remote_fh = dentry->fh->master_fh;

      r = update_local_fh (dir, &name, vol, &parent_fh,
			   fh, &remote_fh, attr);
      free (name.str);
      if (r != ZFS_OK)
	goto out2;
    }
  else
    {
      sattr sa;

#ifdef ENABLE_CHECKING
      if (attr->type != FT_DIR)
	abort ();
#endif

      sa.mode = attr->mode;
      sa.uid = attr->uid;
      sa.gid = attr->gid;
      sa.size = (uint64_t) -1;
      sa.atime = (zfs_time) -1;
      sa.mtime = (zfs_time) -1;

      release_dentry (dir);
      zfsd_mutex_unlock (&vol->mutex);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      r = local_setattr (&dir->fh->attr, dir, &sa, vol);
      if (r != ZFS_OK)
	goto out2;
    }

  if (attr->type == FT_REG)
    {
      /* Schedule update of regular file.  */

      zfsd_mutex_lock (&running_mutex);
      if (update_pool.main_thread == 0)
	{
	  /* Update threads are not running.  */
	  zfsd_mutex_unlock (&running_mutex);
	  return ZFS_OK;
	}
      zfsd_mutex_unlock (&running_mutex);

      r2 = zfs_fh_lookup (fh, NULL, &dentry, NULL);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      if (dentry->fh->flags)
	{
	  dentry->fh->flags |= IFH_UPDATE;
	}
      else
	{
	  dentry->fh->flags |= IFH_UPDATE;
	  zfsd_mutex_lock (&update_queue.mutex);
	  queue_put (&update_queue, &dentry->fh->local_fh);
	  zfsd_mutex_unlock (&update_queue.mutex);
	}
      release_dentry (dentry);
    }

  if (attr->type != FT_DIR)
    return ZFS_OK;

  r = full_local_readdir (fh, &local_entries);
  if (r != ZFS_OK)
    goto out2;

  r = full_remote_readdir (fh, &remote_entries);
  if (r != ZFS_OK)
    {
      htab_destroy (local_entries.htab);
      goto out2;
    }

  HTAB_FOR_EACH_SLOT (local_entries.htab, slot,
    {
      entry = (dir_entry *) *slot;
      slot2 = htab_find_slot (remote_entries.htab, entry, NO_INSERT);
      if (slot2)
	{
	  /* Update file.  */

	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  r = local_lookup (&local_res, dir, &entry->name, vol);
	  if (r != ZFS_OK)
	    goto out;

	  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  r = remote_lookup (&remote_res, dir, &entry->name, vol);
	  if (r != ZFS_OK)
	    goto out;

	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  dentry = get_dentry (&local_res.file, &remote_res.file, vol,
			       dir, entry->name.str, &local_res.attr);
	  release_dentry (dir);

	  if (UPDATE_P (dentry, remote_res.attr))
	    {
	      r = update_local_fh (dentry, &entry->name, vol, fh,
				   &local_res.file, &remote_res.file,
				   &remote_res.attr);
	      if (r != ZFS_OK)
		goto out;
	    }
	  else
	    {
	      release_dentry (dentry);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);
	    }

	  htab_clear_slot (remote_entries.htab, slot2);
	}
      else
	{
	  /* Delete file.  */
	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  if (!delete_tree_name (dir, entry->name.str, vol))
	    goto out;
	}
      htab_clear_slot (local_entries.htab, slot);
    });

  HTAB_FOR_EACH_SLOT (remote_entries.htab, slot,
    {
      /* Create file.  */

      entry = (dir_entry *) *slot;

      r2 = zfs_fh_lookup (fh, &vol, &dir, NULL);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      r = remote_lookup (&remote_res, dir, &entry->name, vol);
      if (r != ZFS_OK)
	goto out;

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      r = create_local_fh (dir, &entry->name, vol, fh, &local_res.file,
			   &remote_res.file, &remote_res.attr);
      if (r != ZFS_OK)
	goto out;

      htab_clear_slot (remote_entries.htab, slot);
    });

  r = ZFS_OK;

out:
  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (!set_metadata (vol, dir->fh, r == ZFS_OK ? METADATA_COMPLETE : 0,
		     attr->version, attr->version))
    {
      vol->flags |= VOLUME_DELETE;
    }
  if (r != ZFS_OK)
    internal_dentry_unlock (dir);
  else
    release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  htab_destroy (local_entries.htab);
  htab_destroy (remote_entries.htab);
  return r;

out2:
  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  internal_dentry_unlock (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  return r;
}

/* Initialize update thread T.  */

static void
update_worker_init (thread *t)
{
  dc_create (&t->dc_call, ZFS_MAX_REQUEST_LEN);
}

/* Cleanup update thread DATA.  */

static void
update_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (&t->dc_call);
}

/* The main function of an update thread.  */

static void *
update_worker (void *data)
{
  thread *t = (thread *) data;

  thread_disable_signals ();

  pthread_cleanup_push (update_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);

  while (1)
    {
      /* Wait until network_dispatch wakes us up.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (get_thread_state (t) == THREAD_DYING)
	break;

     /* TODO: do some work.  */
#if 0
  zfs_fh *fh = (zfs_fh *) data;
  volume vol;
  internal_dentry dentry;
  int32_t r;

  r = zfs_fh_lookup (fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return NULL;
#endif

      /* Put self to the idle queue if not requested to die meanwhile.  */
      zfsd_mutex_lock (&update_pool.idle.mutex);
      if (get_thread_state (t) == THREAD_BUSY)
	{
	  queue_put (&update_pool.idle, &t->index);
	  set_thread_state (t, THREAD_IDLE);
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (get_thread_state (t) != THREAD_DYING)
	    abort ();
#endif
	  zfsd_mutex_unlock (&update_pool.idle.mutex);
	  break;
	}
      zfsd_mutex_unlock (&update_pool.idle.mutex);
    }
  
  pthread_cleanup_pop (1);

  return NULL;
}

/* Main function if the main update thread.  */

static void *
update_main (ATTRIBUTE_UNUSED void *data)
{
  zfs_fh fh;
  size_t index;

  thread_disable_signals ();

  while (!thread_pool_terminate_p (&update_pool))
    {
      bool succeeded;

      /* Get the file handle.  */
      zfsd_mutex_lock (&update_queue.mutex);
      succeeded = queue_get (&update_queue, &fh);
      zfsd_mutex_unlock (&update_queue.mutex);
      if (!succeeded)
	break;

      zfsd_mutex_lock (&update_pool.idle.mutex);

      /* Regulate the number of threads.  */
      if (update_pool.idle.nelem == 0)
	thread_pool_regulate (&update_pool);

      queue_get (&update_pool.idle, &index);
#ifdef ENABLE_CHECKING
      if (get_thread_state (&update_pool.threads[index].t) == THREAD_BUSY)
	abort ();
#endif
      set_thread_state (&update_pool.threads[index].t, THREAD_BUSY);
      update_pool.threads[index].t.u.update.fh = fh;

      /* Let the thread run.  */
      semaphore_up (&update_pool.threads[index].t.sem, 1);

      zfsd_mutex_unlock (&update_pool.idle.mutex);
    }

  message (2, stderr, "Terminating...\n");
  return NULL;
}

/* Start the main update thread.  */

bool
update_start ()
{
  queue_create (&update_queue, sizeof (zfs_fh), 250);

  if (!thread_pool_create (&update_pool, 16, 4, 8, update_main,
			   update_worker, update_worker_init))
    {
      zfsd_mutex_lock (&update_queue.mutex);
      queue_destroy (&update_queue);
      return false;
    }

  return true;
}

/* Terminate update threads and destroy data structures.  */

void
update_cleanup ()
{
  thread_pool_destroy (&update_pool);
  zfsd_mutex_lock (&update_queue.mutex);
  queue_destroy (&update_queue);
}
