/* Functions for updating files.
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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "pthread.h"
#include "update.h"
#include "md5.h"
#include "log.h"
#include "volume.h"
#include "fh.h"
#include "cap.h"
#include "varray.h"
#include "interval.h"
#include "zfs_prot.h"
#include "file.h"
#include "dir.h"

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

/* Update some of the BLOCKS (described in ARGS) of local file LOCAL_CAP
   from remote file REMOTE_CAP.  If USE_BUFFER is true, load the needed
   intervals to BUFFER (the beginning of buffer is at file offset OFFSET) and
   store the updated number of bytes read from file to RCOUNT.  */

static int32_t
update_file_blocks_1 (bool use_buffer, uint32_t *rcount, void *buffer,
		      uint64_t offset, md5sum_args *args, zfs_cap *local_cap,
		      zfs_cap *master_cap, varray *blocks)
{
  volume vol;
  internal_dentry dentry;
  md5sum_res local_md5;
  md5sum_res remote_md5;
  int32_t r;
  unsigned int i, j;

#ifdef ENABLE_CHECKING
  if (VIRTUAL_FH_P (local_cap->fh))
    abort ();
#endif

  args->cap = *master_cap;
  r = remote_md5sum (&remote_md5, args);
  if (r != ZFS_OK)
    return r;

  if (remote_md5.count == 0)
    return ZFS_OK;

  args->cap = *local_cap;
  r = local_md5sum (&local_md5, args);
  if (r != ZFS_OK)
    return r;

  r = zfs_fh_lookup (&local_cap->fh, &vol, &dentry, NULL);
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
  zfsd_mutex_unlock (&dentry->fh->mutex);

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

	      r = full_remote_read (&remote_md5.length[i], buf, local_cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;

	      r = full_local_write (&count, buf, local_cap,
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

		  r = full_local_read (&count, buf, local_cap,
				       remote_md5.offset[i],
				       remote_md5.length[i]);
		  if (r != ZFS_OK)
		    return r;
		  if (count != remote_md5.length[i])
		    abort (); /* FIXME */
		}

	      r = full_remote_read (&remote_md5.length[i], tmp_buf, local_cap,
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
	      r = full_local_write (&count, buf, local_cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		return r;
	      if (count != remote_md5.length[i])
		abort ();	/* FIXME */

	      /* Add the interval to UPDATED.  */
	      r = zfs_fh_lookup (&local_cap->fh, &vol, &dentry, NULL);
	      if (r != ZFS_OK)
		return r;

	      interval_tree_insert (dentry->fh->updated, remote_md5.offset[i],
				    (remote_md5.offset[i]
				     + remote_md5.length[i]));
	      zfsd_mutex_unlock (&dentry->fh->mutex);
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
		    uint64_t offset, internal_cap cap, varray *blocks)
{
  md5sum_args args;
  zfs_cap local_cap;
  zfs_cap master_cap;
  int32_t r;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&cap->mutex);
#ifdef ENABLE_CHECKING
  if (VARRAY_USED (*blocks) == 0)
    abort ();
#endif

  local_cap = cap->local_cap;
  master_cap = cap->master_cap;
  zfsd_mutex_unlock (&cap->mutex);

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
					    &args, &local_cap, &master_cap,
					    blocks);
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
				&local_cap, &master_cap, blocks);
      if (r != ZFS_OK)
	return r;
    }

  return ZFS_OK;
}

/* Return true if file DENTRY on volume VOL should be updated.  */

bool
update_p (internal_dentry dentry, volume vol)
{
  fattr attr;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(vol->local_path && vol->master != this_node))
    abort ();
#endif

  r = refresh_master_fh (dentry, vol);
  if (r != ZFS_OK)
    return false;

  r = remote_getattr (&attr, dentry, vol);
  if (r != ZFS_OK)
    return false;

  /* Update the file if local file was not modified and
     remote file was modified since we updated it last time
     OR the file has not been completelly updated
     and local or remote file was not modified (i.e. handle
     partially updated file).  */
  return ((dentry->fh->attr.version == dentry->fh->meta.master_version
	   && attr.version > dentry->fh->meta.master_version)
	  || (!(dentry->fh->meta.flags & METADATA_COMPLETE)
	      && (dentry->fh->attr.version == dentry->fh->meta.master_version
		  || attr.version == dentry->fh->meta.master_version)));
}

/* Schedule update of regular file DENTRY on volume VOL.  */

static int32_t
schedule_update_regular_file (internal_dentry dentry, volume vol)
{
  int32_t r;

  r = refresh_master_fh (dentry, vol);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&dentry->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return r;
    }

  /* TODO: schedule update of file.  */

  zfsd_mutex_unlock (&dentry->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return ZFS_OK;
}

/* Update the directory DENTRY on volume VOL.
   If SCHEDULE is true schedule update of files.  */

static int32_t
update_directory (internal_dentry dentry, volume vol, bool schedule)
{
  int32_t r;

  r = refresh_master_fh (dentry, vol);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&dentry->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return r;
    }

  /* TODO: update the directory.  */

  zfsd_mutex_unlock (&dentry->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return ZFS_OK;
}

/* Delete file in place of file DENTRY on volume VOL.  */

static bool
delete_tree (internal_dentry dentry, volume vol)
{
  char *path;
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  path = build_local_path (vol, dentry);
  zfsd_mutex_unlock (&dentry->fh->mutex);
  r = recursive_unlink (path, vol);
  zfsd_mutex_unlock (&vol->mutex);
  free (path);

  return r;
}

/* Update file DENTRY of any type on volume VOL.
   If SCHEDULE is true schedule update of file.  */

int32_t
update_file (internal_dentry dentry, volume vol, bool schedule)
{
  internal_dentry dir;
  string name;
  zfs_fh fh;
  fattr attr;
  sattr sa;
  int32_t r;
  read_link_res link_to;
  dir_op_res dir_res;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(vol->local_path && vol->master != this_node))
    abort ();
#endif

  r = remote_getattr (&attr, dentry, vol);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&dentry->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return r;
    }

#ifdef ENABLE_CHECKING
  if (attr.type != FT_DIR && !dentry->parent)
    abort ();
#endif

  switch (attr.type)
    {
      default:
      case FT_BAD:
	if (!delete_tree (dentry, vol))
	  {
	    zfsd_mutex_unlock (&dentry->fh->mutex);
	    zfsd_mutex_unlock (&vol->mutex);
	    return ZFS_UPDATE_FAILED;
	  }
	zfsd_mutex_unlock (&vol->mutex);
	return ZFS_OK;

      case FT_REG:
	if (attr.type != dentry->fh->attr.type)
	  {
	    create_res res;
	    int fd;

	    dir = dentry->parent;
	    xmkstring (&name, dentry->name);
	    if (!delete_tree (dentry, vol))
	      {
		zfsd_mutex_unlock (&dentry->fh->mutex);
		zfsd_mutex_unlock (&vol->mutex);
		free (name.str);
		return ZFS_UPDATE_FAILED;
	      }

	    sa.mode = attr.mode;
	    sa.uid = attr.uid;
	    sa.gid = attr.gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    zfsd_mutex_lock (&dir->fh->mutex);
	    r = local_create (&res, &fd, dir, &name,
			      O_CREAT | O_WRONLY | O_TRUNC, &sa, vol);
	    if (r == ZFS_OK)
	      {
		close (fd);
		dentry = internal_dentry_create (&res.file, &undefined_fh, vol,
						 dir, name.str, &res.attr);
	      }
	    zfsd_mutex_unlock (&dir->fh->mutex);
	    free (name.str);
	    if (r != ZFS_OK)
	      {
		zfsd_mutex_unlock (&vol->mutex);
		return r;
	      }
	  }
	else
	  {
	    sa.mode = attr.mode;
	    sa.uid = attr.uid;
	    sa.gid = attr.gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_setattr (&dentry->fh->attr, dentry, &sa, vol);
	    if (r != ZFS_OK)
	      {
		zfsd_mutex_unlock (&dentry->fh->mutex);
		zfsd_mutex_unlock (&vol->mutex);
		return r;
	      }
	  }

	if (!schedule)
	  return ZFS_OK;
	return schedule_update_regular_file (dentry, vol);

      case FT_DIR:
	if (attr.type != dentry->fh->attr.type)
	  {
	    dir_op_res res;

#ifdef ENABLE_CHECKING
	    if (!dentry->parent)
	      abort ();
#endif

	    dir = dentry->parent;
	    xmkstring (&name, dentry->name);
	    if (!delete_tree (dentry, vol))
	      {
		zfsd_mutex_unlock (&dentry->fh->mutex);
		zfsd_mutex_unlock (&vol->mutex);
		free (name.str);
		return ZFS_UPDATE_FAILED;
	      }

	    sa.mode = attr.mode;
	    sa.uid = attr.uid;
	    sa.gid = attr.gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    zfsd_mutex_lock (&dir->fh->mutex);
	    r = local_mkdir (&res, dir, &name, &sa, vol);
	    if (r == ZFS_OK)
	      dentry = internal_dentry_create (&res.file, &undefined_fh, vol,
					       dir, name.str, &res.attr);
	    zfsd_mutex_unlock (&dir->fh->mutex);
	    free (name.str);
	    if (r != ZFS_OK)
	      {
		zfsd_mutex_unlock (&vol->mutex);
		return r;
	      }
	  }
	else
	  {
	    sa.mode = attr.mode;
	    sa.uid = attr.uid;
	    sa.gid = attr.gid;
	    sa.size = (uint64_t) -1;
	    sa.atime = (zfs_time) -1;
	    sa.mtime = (zfs_time) -1;

	    r = local_setattr (&dentry->fh->attr, dentry, &sa, vol);
	    if (r != ZFS_OK)
	      {
		zfsd_mutex_unlock (&dentry->fh->mutex);
		zfsd_mutex_unlock (&vol->mutex);
		return r;
	      }
	  }
	return update_directory (dentry, vol, schedule);

      case FT_LNK:
	fh = dentry->fh->local_fh;
	r = remote_readlink (&link_to, dentry->fh, vol);
	zfsd_mutex_unlock (&dentry->fh->mutex);
	if (r != ZFS_OK)
	  return r;

	r = zfs_fh_lookup (&fh, &vol, &dentry, NULL);
	if (r != ZFS_OK)
	  return r;

	dir = dentry->parent;
	xmkstring (&name, dentry->name);
	if (!delete_tree (dentry, vol))
	  {
	    zfsd_mutex_unlock (&dentry->fh->mutex);
	    zfsd_mutex_unlock (&vol->mutex);
	    free (name.str);
	    return ZFS_UPDATE_FAILED;
	  }

	sa.mode = (uint32_t) -1;
	sa.uid = attr.uid;
	sa.gid = attr.gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	zfsd_mutex_lock (&dir->fh->mutex);
	r = local_symlink (&dir_res, dir, &name, &link_to.path, &sa, vol);
	zfsd_mutex_unlock (&dir->fh->mutex);
	zfsd_mutex_unlock (&vol->mutex);
	free (name.str);
	return r;

      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	dir = dentry->parent;
	xmkstring (&name, dentry->name);
	if (!delete_tree (dentry, vol))
	  {
	    zfsd_mutex_unlock (&dentry->fh->mutex);
	    zfsd_mutex_unlock (&vol->mutex);
	    free (name.str);
	    return ZFS_UPDATE_FAILED;
	  }

	sa.mode = attr.mode;
	sa.uid = attr.uid;
	sa.gid = attr.gid;
	sa.size = (uint64_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;

	zfsd_mutex_lock (&dir->fh->mutex);
	r = local_mknod (&dir_res, dir, &name, &sa, attr.type, attr.rdev, vol);
	zfsd_mutex_unlock (&dir->fh->mutex);
	zfsd_mutex_unlock (&vol->mutex);
	free (name.str);
	return r;
    }

  return ZFS_OK;
}
