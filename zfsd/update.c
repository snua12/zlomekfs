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
#include "journal.h"
#include "metadata.h"

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
  interval_tree_complement_varray (fh->modified, &tmp, blocks);
  varray_destroy (&tmp);
}

/* Update BLOCKS (described in ARGS) of local file CAP from remote file.  */

static int32_t
update_file_blocks_1 (md5sum_args *args, zfs_cap *cap, varray *blocks)
{
  bool flush;
  volume vol;
  internal_dentry dentry;
  md5sum_res local_md5;
  md5sum_res remote_md5;
  int32_t r;
  unsigned int i, j;

#ifdef ENABLE_CHECKING
  if (!REGULAR_FH_P (cap->fh))
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

  r = zfs_fh_lookup_nolock (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r != ZFS_OK)
    abort ();
#endif

#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node))
    abort ();
#endif

  /* If the size of remote file differs from the size of local file
     truncate local file.  */
  if (local_md5.size != remote_md5.size)
    {
      sattr sa;

      memset (&sa, -1, sizeof (sattr));
      sa.size = (remote_md5.offset[remote_md5.count - 1]
		 + remote_md5.length[remote_md5.count - 1]);

      r = local_setattr (&dentry->fh->attr, dentry, &sa, vol);
      if (r != ZFS_OK)
	return r;

      r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

      flush = (local_md5.size < remote_md5.size
	       && (dentry->fh->meta.flags & METADATA_COMPLETE));

      local_md5.size = remote_md5.size;
      interval_tree_delete (dentry->fh->updated, local_md5.size, UINT64_MAX);
      interval_tree_delete (dentry->fh->modified, local_md5.size, UINT64_MAX);

      if (flush || dentry->fh->updated->deleted)
	{
	  if (!flush_interval_tree (vol, dentry->fh, METADATA_TYPE_UPDATED))
	    vol->delete_p = true;
	}

      if (local_md5.count > remote_md5.count)
	local_md5.count = remote_md5.count;
    }
  else
    {
      zfsd_mutex_unlock (&fh_mutex);
    }

  /* Delete the same blocks from MODIFIED interval tree and add them to
     UPDATED interval tree.  */
  flush = dentry->fh->modified->deleted;
  for (i = 0; i < local_md5.count; i++)
    {
      if (local_md5.offset[i] != remote_md5.offset[i])
	{
	  abort (); /* FIXME: do not abort, return error only */
	  return ZFS_UPDATE_FAILED;
	}

      if (local_md5.length[i] == remote_md5.length[i]
	  && memcmp (local_md5.md5sum[i], remote_md5.md5sum[i], MD5_SIZE) == 0)
	{
	  interval_tree_delete (dentry->fh->modified, local_md5.offset[i],
				local_md5.offset[i] + local_md5.length[i]);
	  flush |= dentry->fh->modified->deleted;
	  if (!append_interval (vol, dentry->fh, METADATA_TYPE_UPDATED,
				local_md5.offset[i],
				local_md5.offset[i] + local_md5.length[i]))
	    vol->delete_p = true;
	}
    }
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* Update different blocks.  */
  for (i = 0, j = 0; i < remote_md5.count; i++)
    {
      if (remote_md5.length[i] > ZFS_MAXDATA
	  || remote_md5.offset[i] + remote_md5.length[i] > remote_md5.size)
	{
	  abort (); /* FIXME: do not abort, return error only */
	  return ZFS_UPDATE_FAILED;
	}

      if (i >= local_md5.count
	  || local_md5.length[i] != remote_md5.length[i]
	  || memcmp (local_md5.md5sum[i], remote_md5.md5sum[i], MD5_SIZE) != 0)
	{
	  uint32_t count;
	  char buf[ZFS_MAXDATA];
	  char buf2[ZFS_MAXDATA];

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

	      r = full_remote_read (&remote_md5.length[i], buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      r = full_local_write (&count, buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;
	    }
	  else
	    {
	      /* MD5 block is larger than block(s) to be updated.  */

	      r = full_remote_read (&remote_md5.length[i], buf2, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      r = full_local_read (&count, buf, cap,
				   remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      /* Copy the part which was not written from local file
		 because local file was truncated meanwhile.  */
	      if (count < remote_md5.length[i])
		memcpy (buf + count, buf2 + count,
			remote_md5.length[i] - count);

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
			  buf2 + start - remote_md5.offset[i],
			  end - start);
		}

	      /* Write updated buffer BUF.  */
	      r = full_local_write (&count, buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;
	    }

	  /* Add the interval to UPDATED.  */
	  r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r != ZFS_OK)
	    abort ();
#endif

	  if (!append_interval (vol, dentry->fh, METADATA_TYPE_UPDATED,
				remote_md5.offset[i],
				remote_md5.offset[i] + count))
	    vol->delete_p = true;

	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	}
    }

  if (flush)
    {
      r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

      if (!flush_interval_tree (vol, dentry->fh, METADATA_TYPE_MODIFIED))
	vol->delete_p = true;

      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
    }

  return ZFS_OK;
}

/* Update BLOCKS of local file CAP from remote file.  */

int32_t
update_file_blocks (zfs_cap *cap, varray *blocks)
{
  md5sum_args args;
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  int32_t r, r2;
  unsigned int i;

  if (VARRAY_USED (*blocks) == 0)
    return ZFS_OK;

  r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* FIXME: call this function only if we have a valid master file handle.  */
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return ENOENT;
    }

  if (zfs_fh_undefined (icap->master_cap.fh)
      || zfs_cap_undefined (icap->master_cap))
    {
      zfs_cap master_cap;

      r = remote_open (&master_cap, icap, O_RDWR, dentry, vol);
      if (r != ZFS_OK)
	return r;

      r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
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
		  r = update_file_blocks_1 (&args, cap, blocks);
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
      r = update_file_blocks_1 (&args, cap, blocks);
      if (r != ZFS_OK)
	return r;
    }

  r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (dentry->fh->meta.flags & METADATA_COMPLETE)
    {
      dentry->fh->flags &= ~IFH_UPDATE;

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

/* Update file with file handle FH.  */

int32_t
update_file (zfs_fh *fh)
{
  varray blocks;
  volume vol;
  internal_dentry dentry;
  internal_cap icap;
  zfs_cap cap;
  int32_t r, r2;
  fattr attr;

  r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
  if (r != ZFS_OK)
    return r;

  /* FIXME: call this function only if we have a valid master file handle.  */
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return ENOENT;
    }

  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node))
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return ZFS_UPDATE_FAILED;
    }
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  cap.fh = *fh;
  cap.flags = O_RDONLY;
  r = get_capability (&cap, &icap, &vol, &dentry, NULL, true, true);
  if (r != ZFS_OK)
    return r;

  r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, NULL, &cap);
  if (r != ZFS_OK)
    return r;

  if (dentry->fh->attr.type != FT_REG)
    {
      r = ZFS_UPDATE_FAILED;
      goto out;
    }

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r2 = zfs_fh_lookup (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  r = remote_getattr (&attr, dentry, vol);
  if (r != ZFS_OK)
    goto out2;

  if (attr.type != FT_REG)
    {
      r = ZFS_UPDATE_FAILED;
      goto out2;
    }

  r2 = zfs_fh_lookup (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (!load_interval_trees (vol, dentry->fh))
    {
      vol->delete_p = true;
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      r = ZFS_METADATA_ERROR;
      goto out2;
    }

  zfsd_mutex_unlock (&vol->mutex);
  get_blocks_for_updating (dentry->fh, 0, attr.size, &blocks);
  release_dentry (dentry);
  r = update_file_blocks (&cap, &blocks);
  varray_destroy (&blocks);

  r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (!save_interval_trees (vol, dentry->fh))
    {
      vol->delete_p = true;
      r = ZFS_METADATA_ERROR;
      goto out;
    }

  /* If the file was not completelly updated add it to queue again.  */
  if (r == ZFS_OK && dentry->fh->flags & IFH_UPDATE)
    {
      zfsd_mutex_lock (&update_queue.mutex);
      queue_put (&update_queue, &dentry->fh->local_fh);
      zfsd_mutex_unlock (&update_queue.mutex);
    }

  goto out;

out2:
  r2 = find_capability_nolock (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

out:
  put_capability (icap, dentry->fh, NULL);
  internal_cap_unlock (vol, dentry, NULL);
  return r;
}

/* Return true if file *DENTRYP on volume *VOLP with file handle FH should
   be updated.  Store remote attributes to ATTR.  */

int
update_p (volume *volp, internal_dentry *dentryp, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;

  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!((*volp)->local_path.str && (*volp)->master != this_node))
    abort ();
#endif

  /* FIXME: call this function only if we have a valid master file handle.  */
  if (zfs_fh_undefined ((*dentryp)->fh->meta.master_fh))
    return 0;

  release_dentry (*dentryp);
  zfsd_mutex_unlock (&(*volp)->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r2 = zfs_fh_lookup (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  r = remote_getattr (attr, *dentryp, *volp);
  if (r != ZFS_OK)
    goto out;

  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  return (UPDATE_P (*dentryp, *attr) * IFH_UPDATE
	  + REINTEGRATE_P (*dentryp) * IFH_REINTEGRATE);

out:
  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  return 0;
}

/* Delete file in place of file DENTRY on volume VOL.  */

bool
delete_tree (internal_dentry dentry, volume vol)
{
  string path;
  uint32_t vid;
  bool r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  build_local_path (&path, vol, dentry);
  vid = vol->id;
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = recursive_unlink (&path, vid, false);
  free (path.str);

  return r;
}

/* Delete file NAME in directory DIR on volume VOL.  */

bool
delete_tree_name (internal_dentry dir, string *name, volume vol)
{
  string path;
  uint32_t vid;
  bool r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  vid = vol->id;
  release_dentry (dir);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  r = recursive_unlink (&path, vid, false);
  free (path.str);

  return r;
}

/* Move file FH from shadow on volume VOL to file NAME in directory DIR.  */

bool
move_from_shadow (volume vol, zfs_fh *fh, internal_dentry dir, string *name)
{
  string path;
  string shadow_path;
  uint32_t vid;
  uint32_t new_parent_dev;
  uint32_t new_parent_ino;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  vid = vol->id;
  new_parent_dev = dir->fh->local_fh.dev;
  new_parent_ino = dir->fh->local_fh.ino;
  release_dentry (dir);
  zfsd_mutex_unlock (&fh_mutex);
  get_shadow_path (&shadow_path, vol, fh, false);
  zfsd_mutex_unlock (&vol->mutex);

  if (!recursive_unlink (&path, vid, false))
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  if (rename (shadow_path.str, path.str) != 0)
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  vol = volume_lookup (vid);
  if (!vol)
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  if (!metadata_hardlink_replace (vol, fh, 0, 0, &empty_string, new_parent_dev,
				  new_parent_ino, name))
    {
      zfsd_mutex_unlock (&vol->mutex);
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  zfsd_mutex_unlock (&vol->mutex);
  free (path.str);
  free (shadow_path.str);
  return true;
}

/* Move file DENTRY on volume VOL to shadow.  */

bool
move_to_shadow (volume vol, zfs_fh *fh, internal_dentry dir, string *name)
{
  string path;
  string shadow_path;
  uint32_t vid;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  vid = vol->id;
  release_dentry (dir);
  zfsd_mutex_unlock (&fh_mutex);
  get_shadow_path (&shadow_path, vol, fh, true);
  zfsd_mutex_unlock (&vol->mutex);

  if (!recursive_unlink (&shadow_path, vid, true))
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  if (rename (path.str, shadow_path.str) != 0)
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  vol = volume_lookup (vid);
  if (!vol)
    {
      free (path.str);
      free (shadow_path.str);
      return false;
    }

  if (!metadata_hardlink_set_shadow (vol, fh))
    {
      zfsd_mutex_unlock (&vol->mutex);
      free (path.str);
      free (shadow_path.str);
      return false;
    }
  
  zfsd_mutex_unlock (&vol->mutex);
  free (path.str);
  free (shadow_path.str);
  return true;
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
  metadata meta;
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
	r = ZFS_OK;
	break;

      case FT_REG:
	if (remote_attr->type != dentry->fh->attr.type)
	  {
	    int fd;

	    if (!delete_tree (dentry, vol))
	      return ZFS_UPDATE_FAILED;

	    r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
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
			      O_CREAT | O_WRONLY | O_TRUNC, &sa, vol, &meta);
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
	    if (r == ZFS_OK)
	      meta = dentry->fh->meta;
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

	    r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
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

	    r = local_mkdir (&res, dir, name, &sa, vol, &meta);
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
	    if (r == ZFS_OK)
	      meta = dentry->fh->meta;
	  }
	break;

      case FT_LNK:
	zfsd_mutex_unlock (&fh_mutex);
	r = remote_readlink (&link_to, dentry, vol);
	if (r != ZFS_OK)
	  return r;

	r2 = zfs_fh_lookup_nolock (local_fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	if (!delete_tree (dentry, vol))
	  return ZFS_UPDATE_FAILED;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
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

	r = local_symlink (&res, dir, name, &link_to.path, &sa, vol, &meta);
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

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
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
			 remote_attr->rdev, vol, &meta);
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
	  r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();
	}
      else
	{
	  dir = NULL;
	  zfsd_mutex_lock (&fh_mutex);
	}

      dentry = get_dentry (local_fh, remote_fh, vol, dir, name,
			   local_attr, &meta);
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
	  vol->delete_p = true;
	  r = ZFS_METADATA_ERROR;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }

  return r;
}

/* Create local generic file NAME in directory DIR on volume VOL with remote
   file REMOTE_FH and remote attributes REMOTE_ATTR.  DIR_FH is a file handle
   of the directory.  */

static int32_t
create_local_fh (internal_dentry dir, string *name, volume vol,
		 zfs_fh *dir_fh, zfs_fh *remote_fh, fattr *remote_attr)
{
  internal_dentry dentry;
  zfs_fh *local_fh;
  fattr *local_attr;
  sattr sa;
  metadata meta;
  int32_t r, r2;
  read_link_res link_to;
  create_res cr_res;
  dir_op_res res;
  int fd;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  sa.mode = remote_attr->mode;
  sa.uid = remote_attr->uid;
  sa.gid = remote_attr->gid;
  sa.size = (uint64_t) -1;
  sa.atime = remote_attr->atime;
  sa.mtime = remote_attr->mtime;

  local_fh = &res.file;
  local_attr = &res.attr;
  switch (remote_attr->type)
    {
      default:
	abort ();

      case FT_BAD:
	release_dentry (dir);
	zfsd_mutex_unlock (&vol->mutex);
	zfsd_mutex_unlock (&fh_mutex);
	r = ZFS_OK;
	break;

      case FT_REG:
	r = local_create (&cr_res, &fd, dir, name,
			  O_CREAT | O_WRONLY | O_TRUNC, &sa, vol, &meta);
	if (r == ZFS_OK)
	  {
	    close (fd);
	    local_fh = &cr_res.file;
	    local_attr = &cr_res.attr;
	  }
	break;

      case FT_DIR:
	r = local_mkdir (&res, dir, name, &sa, vol, &meta);
	break;

      case FT_LNK:
	release_dentry (dir);
	zfsd_mutex_unlock (&fh_mutex);

	r = remote_readlink_zfs_fh (&link_to, remote_fh, vol);
	if (r != ZFS_OK)
	  return r;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	r = local_symlink (&res, dir, name, &link_to.path, &sa, vol, &meta);
	break;

      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	r = local_mknod (&res, dir, name, &sa, remote_attr->type,
			 remote_attr->rdev, vol, &meta);
	break;
    }

  if (r == ZFS_OK)
    {
      uint32_t flags;
      bool ok;

      r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      dentry = get_dentry (local_fh, remote_fh, vol, dir, name,
			   local_attr, &meta);
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
	  vol->delete_p = true;
	  r = ZFS_METADATA_ERROR;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }

  return r;
}

/* Create remote generic file NAME in directory DIR on volume VOL according
   to local attributes ATTR.  DIR_FH is a file handle of the directory.
   Return file handle together with attributes in RES.  */

static int32_t
create_remote_fh (dir_op_res *res, internal_dentry dir, string *name,
		  volume vol, zfs_fh *dir_fh, fattr *attr)
{
  sattr sa;
  int32_t r, r2;
  read_link_res link_to;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  sa.mode = attr->mode;
  sa.uid = attr->uid;
  sa.gid = attr->gid;
  sa.size = (uint64_t) -1;
  sa.atime = attr->atime;
  sa.mtime = attr->mtime;

  switch (attr->type)
    {
      default:
	abort ();

      case FT_DIR:
	zfsd_mutex_unlock (&fh_mutex);
	r = remote_mkdir (res, dir, name, &sa, vol);
	break;

      case FT_LNK:
	r = local_readlink_name (&link_to, dir, name, vol);
	if (r != ZFS_OK)
	  return r;

	r2 = zfs_fh_lookup (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	r = remote_symlink (res, dir, name, &link_to.path, &sa, vol);
	break;

      case FT_REG:
      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	zfsd_mutex_unlock (&fh_mutex);
	r = remote_mknod (res, dir, name, &sa, attr->type, attr->rdev, vol);
	break;
    }

  return r;
}

/* Update the directory DIR on volume VOL with file handle FH,
   set attributes according to ATTR.  */

static int32_t
update_fh (volume vol, internal_dentry dir, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;
  internal_dentry dentry, parent;
  filldir_htab_entries local_entries, remote_entries;
  dir_op_res local_res, remote_res;
  metadata meta;
  dir_entry *entry;
  void **slot, **slot2;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dir->fh) && vol->master != this_node))
    abort ();
  if (attr->type != dir->fh->attr.type && !dir->parent)
    abort ();
#endif

  parent = dir->parent;
  if (parent)
    {
      zfsd_mutex_lock (&dir->parent->fh->mutex);
      if (CONFLICT_DIR_P (parent->fh->local_fh))
	parent = parent->parent;
      zfsd_mutex_unlock (&dir->parent->fh->mutex);
    }

  zfsd_mutex_unlock (&fh_mutex);

  if (parent)
    {
      zfs_fh parent_fh;
      string name;
      zfs_fh remote_fh;

      release_dentry (dir);
      zfsd_mutex_unlock (&vol->mutex);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
      if (!dir->parent)
	abort ();
#endif
      zfsd_mutex_lock (&dir->parent->fh->mutex);
      if (CONFLICT_DIR_P (dir->parent->fh->local_fh))
	{
#ifdef ENABLE_CHECKING
	  if (!dir->parent->parent)
	    abort ();
#endif
	  zfsd_mutex_lock (&dir->parent->parent->fh->mutex);
	  parent_fh = dir->parent->parent->fh->local_fh;
	  zfsd_mutex_unlock (&dir->parent->parent->fh->mutex);
	}
      else
	parent_fh = dir->parent->fh->local_fh;
      zfsd_mutex_unlock (&dir->parent->fh->mutex);

      xstringdup (&name, &dir->name);
      remote_fh = dir->fh->meta.master_fh;

      r = update_local_fh (dir, &name, vol, &parent_fh,
			   fh, &remote_fh, attr);
      free (name.str);
      if (r != ZFS_OK)
	return r;
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

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      r = local_setattr (&dir->fh->attr, dir, &sa, vol);
      if (r != ZFS_OK)
	return r;
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

      r2 = zfs_fh_lookup (fh, NULL, &dentry, NULL, false);
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
    return r;

  r = full_remote_readdir (fh, &remote_entries);
  if (r != ZFS_OK)
    {
      htab_destroy (local_entries.htab);
      return r;
    }

  HTAB_FOR_EACH_SLOT (local_entries.htab, slot,
    {
      entry = (dir_entry *) *slot;
      slot2 = htab_find_slot (remote_entries.htab, entry, NO_INSERT);
      if (slot2)
	{
	  /* Update file.  */

	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  r = local_lookup (&local_res, dir, &entry->name, vol, &meta);
	  if (r != ZFS_OK)
	    goto out;

	  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  r = remote_lookup (&remote_res, dir, &entry->name, vol);
	  if (r != ZFS_OK)
	    goto out;

	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  dentry = get_dentry (&local_res.file, &remote_res.file, vol,
			       dir, &entry->name, &local_res.attr, &meta);
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
	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
	  if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	    abort ();

	  if (!delete_tree_name (dir, &entry->name, vol))
	    goto out;
	}
      htab_clear_slot (local_entries.htab, slot);
    });

  HTAB_FOR_EACH_SLOT (remote_entries.htab, slot,
    {
      /* Create file.  */

      entry = (dir_entry *) *slot;

      r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      r = remote_lookup (&remote_res, dir, &entry->name, vol);
      if (r != ZFS_OK)
	goto out;

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
      if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)
	abort ();

      r = create_local_fh (dir, &entry->name, vol, fh,
			   &remote_res.file, &remote_res.attr);
      if (r != ZFS_OK)
	goto out;

      htab_clear_slot (remote_entries.htab, slot);
    });

  r = ZFS_OK;

out:
  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (!set_metadata (vol, dir->fh, r == ZFS_OK ? METADATA_COMPLETE : 0,
		     attr->version, attr->version))
    vol->delete_p = true;

  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  htab_destroy (local_entries.htab);
  htab_destroy (remote_entries.htab);
  return r;
}

/* Update generic file DENTRY on volume VOL with file handle FH according
   to attributes ATTR.  */

static int32_t
reintegrate_fh (volume vol, internal_dentry dentry, zfs_fh *fh)
{
  int32_t r, r2;
  bool b;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node))
    abort ();
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  if (dentry->fh->attr.type == FT_DIR)
    {
      internal_dentry conflict;
      journal_entry entry, next;
      dir_op_res local_res;
      dir_op_res res;
      metadata meta;
      file_info_res info;
      bool flush_journal;

#ifdef ENABLE_CHECKING
      if (dentry->fh->level == LEVEL_UNLOCKED)
	abort ();
#endif

      flush_journal = false;
      for (entry = dentry->fh->journal->first; entry; entry = next)
	{
	  next = entry->next;

	  CHECK_MUTEX_LOCKED (&fh_mutex);
	  CHECK_MUTEX_LOCKED (&vol->mutex);
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  switch (entry->oper)
	    {
	      case JOURNAL_OPERATION_ADD:
		/* Check whether local file still exists.  */ 
		r = local_lookup (&local_res, dentry, &entry->name, vol,
				  &meta);
		r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
		  abort ();
#endif
		if (r != ZFS_OK)
		  {
		    if (!journal_delete_entry (dentry->fh->journal, entry))
		      abort ();
		    flush_journal = true;
		    continue;
		  }

		zfsd_mutex_unlock (&fh_mutex);
		r = remote_lookup (&res, dentry, &entry->name, vol);
		r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
		  abort ();
#endif

		if (r == ZFS_OK)
		  {
		    if (local_res.attr.type != FT_DIR
			|| res.attr.type != FT_DIR)
		      {
			conflict = create_conflict (vol, dentry, &entry->name,
						    &local_res.file,
						    &local_res.attr);
			add_file_to_conflict_dir (vol, conflict, true,
						  &local_res.file,
						  &local_res.attr, &meta);
			add_file_to_conflict_dir (vol, conflict, true,
						  &res.file, &res.attr, NULL);
			release_dentry (conflict);
		      }
		  }
		else if (r == ENOENT || r == ESTALE)
		  {
		    if (zfs_fh_undefined (entry->master_fh))
		      {
			internal_dentry subdentry;

			r = create_remote_fh (&res, dentry, &entry->name, vol,
					      fh, &local_res.attr);
			r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r != ZFS_OK)
			  continue;

			/* Update local metadata.  */
			subdentry = dentry_lookup (&local_res.file);
			if (subdentry)
			  meta = subdentry->fh->meta;

			meta.master_fh = res.file;
			meta.master_version = meta.local_version;
			meta.local_version++;

			if (subdentry)
			  {
			    subdentry->fh->meta = meta;
			    set_attr_version (&subdentry->fh->attr,
					      &subdentry->fh->meta);
			    release_dentry (subdentry);
			  }

			if (!flush_metadata (vol, &meta))
			  {
			    vol->delete_p = true;
			    continue;
			  }

			release_dentry (dentry);
			zfsd_mutex_unlock (&fh_mutex);

			/* Set remote metadata. */
			r = remote_reintegrate_set (&res.file,
						    meta.master_version,
						    NULL, vol);

			r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r != ZFS_OK)
			  continue;

			if (!journal_delete_entry (dentry->fh->journal,
						   entry))
			  abort ();
			flush_journal = true;
		      }
		    else
		      {
			release_dentry (dentry);
			zfsd_mutex_unlock (&fh_mutex);
			r = remote_file_info (&info, &entry->master_fh, vol);
			if (r == ZFS_OK)
			  free (info.path.str);

			r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif

			if (r == ZFS_OK)
			  {
			    zfsd_mutex_unlock (&fh_mutex);
			    r = remote_reintegrate_add (vol, dentry,
							&entry->name,
							&entry->master_fh);
			    r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						       false);
#ifdef ENABLE_CHECKING
			    if (r2 != ZFS_OK)
			      abort ();
#endif
			    if (r == ZFS_OK)
			      {
				if (!journal_delete_entry (dentry->fh->journal,
							   entry))
				  abort ();
				flush_journal = true;
			      }
			  }
			else if (r == ENOENT || r == ESTALE)
			  {
			    /* The file does not exists on master.
			       This can happen when we linked/renamed a file
			       while master has deleted it.
			       In this situation, delete the local file.  */
			    b = delete_tree_name (dentry, &entry->name, vol);
			    r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						       false);
#ifdef ENABLE_CHECKING
			    if (r2 != ZFS_OK)
			      abort ();
#endif
			    if (!b)
			      goto out;

			    if (!journal_delete_entry (dentry->fh->journal,
						       entry))
			      abort ();
			    flush_journal = true;
			  }
			else
			  {
			    message (0, stderr,
				     "Reintegrate file info error: %d\n", r);
			    goto out;
			  }
		      }
		  }
		else
		  {
		    message (0, stderr, "Reintegrate lookup error: %d\n", r);
		    goto out;
		  }
		break;

	      case JOURNAL_OPERATION_DEL:
		zfsd_mutex_unlock (&fh_mutex);
		r = remote_lookup (&res, dentry, &entry->name, vol);
		r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
		  abort ();
#endif

		if (r == ZFS_OK)
		  {
		    if (ZFS_FH_EQ (res.file, entry->master_fh))
		      {
			zfs_fh file_fh;

			file_fh.dev = entry->dev;
			file_fh.ino = entry->ino;
			file_fh.gen = entry->gen;
			zfsd_mutex_unlock (&fh_mutex);
			r = local_file_info (&info, &file_fh, vol);
			if (r == ZFS_OK)
			  free (info.path.str);

			r = remote_reintegrate_del (vol, dentry, &entry->name,
						    r != ZFS_OK);
			r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r == ZFS_OK)
			  {
			    if (!journal_delete_entry (dentry->fh->journal,
						       entry))
			      abort ();
			    flush_journal = true;
			  }
		      }
		    else
		      {
			local_res.file.sid = this_node->id;
			conflict = create_conflict (vol, dentry, &entry->name,
						    &res.file, &res.attr);
			add_file_to_conflict_dir (vol, conflict, true,
						  &res.file, &res.attr, NULL);
			add_file_to_conflict_dir (vol, conflict, false,
						  &local_res.file, &res.attr,
						  NULL);
			if (!try_resolve_conflict (conflict))
			  release_dentry (conflict);
		      }
		  }
		else if (r == ENOENT || r == ESTALE)
		  {
		    /* Nothing to do.  */

		    if (!journal_delete_entry (dentry->fh->journal, entry))
		      abort ();
		    flush_journal = true;
		  }
		else
		  {
		    message (0, stderr, "Reintegrate lookup error: %d\n", r);
		    goto out;
		  }
		break;

	      default:
		abort ();
	    }
	}

out:
      zfsd_mutex_unlock (&fh_mutex);
      if (flush_journal)
	{
	  if (!write_journal (vol, dentry->fh))
	    vol->delete_p = true;
	}
    }
  else if (dentry->fh->attr.type == FT_REG)
    {
      /* Schedule reintegration of regular file.  */

      zfsd_mutex_unlock (&fh_mutex);

      zfsd_mutex_lock (&running_mutex);
      if (update_pool.main_thread == 0)
	{
	  /* Update threads are not running.  */
	  zfsd_mutex_unlock (&running_mutex);
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  return ZFS_OK;
	}
      zfsd_mutex_unlock (&running_mutex);

      if (dentry->fh->flags)
	{
	  dentry->fh->flags |= IFH_REINTEGRATE;
	}
      else
	{
	  dentry->fh->flags |= IFH_REINTEGRATE;
	  zfsd_mutex_lock (&update_queue.mutex);
	  queue_put (&update_queue, &dentry->fh->local_fh);
	  zfsd_mutex_unlock (&update_queue.mutex);
	}
    }

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  return ZFS_OK;
}

/* Reintegrate or update generic file DENTRY on volume VOL with file handle FH
   and remote file attributes ATTR.  HOW specifies what we should do.  */

int32_t
update (volume vol, internal_dentry dentry, zfs_fh *fh, fattr *attr, int how)
{
  int32_t r;

  if (dentry->fh->attr.type != attr->type)
    {
#ifdef ENABLE_CHECKING
      if (!dentry->parent)
	abort ();
#endif

      /* This can't happen.  If it happens something is wierd,
	 either someone wants to destabilize us
	 or some metadata was not updated.  */
      abort ();	/* FIXME: delete the abort () after testing */
      return ZFS_UPDATE_FAILED;
    }

  if (how & IFH_REINTEGRATE)
    {
      r = reintegrate_fh (vol, dentry, fh);
      if (r != ZFS_OK)
	return r;

      if (how & IFH_UPDATE)
	{
	  r = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
	  if (r != ZFS_OK)
	    return r;

	  how = update_p (&vol, &dentry, fh, attr);
	  if ((how & (IFH_REINTEGRATE | IFH_UPDATE)) == IFH_UPDATE)
	    {
	      r = update_fh (vol, dentry, fh, attr);
	    }
	  else
	    {
	      release_dentry (dentry);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);
	    }
	}
    }
  else if (how & IFH_UPDATE)
    {
      r = update_fh (vol, dentry, fh, attr);
    }
  else
    abort ();

  return r;
}

/* Initialize update thread T.  */

static void
update_worker_init (thread *t)
{
  t->dc_call = dc_create ();
}

/* Cleanup update thread DATA.  */

static void
update_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  dc_destroy (t->dc_call);
}

/* The main function of an update thread.  */

static void *
update_worker (void *data)
{
  thread *t = (thread *) data;
  lock_info li[MAX_LOCKED_FILE_HANDLES];

  thread_disable_signals ();

  pthread_cleanup_push (update_worker_cleanup, data);
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Network worker thread");
  set_lock_info (li);

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

      update_file ((zfs_fh *) &t->u.update.fh);

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
  pthread_setspecific (thread_name_key, "Update main thread");

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
update_start (void)
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
update_cleanup (void)
{
  thread_pool_destroy (&update_pool);
  zfsd_mutex_lock (&update_queue.mutex);
  queue_destroy (&update_queue);
}
