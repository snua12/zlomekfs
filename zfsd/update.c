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
#include "random.h"
#include "volume.h"
#include "fh.h"
#include "cap.h"
#include "varray.h"
#include "interval.h"
#include "zfs_prot.h"
#include "file.h"
#include "dir.h"
#include "network.h"
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

  TRACE ("");
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

/* Update BLOCKS (described in ARGS) of local file CAP from remote file,
   start searching in BLOCKS at index INDEX.  */

static int32_t
update_file_blocks_1 (md5sum_args *args, zfs_cap *cap, varray *blocks,
		      unsigned int *index)
{
  bool flush;
  volume vol;
  internal_dentry dentry;
  md5sum_res local_md5;
  md5sum_res remote_md5;
  int32_t r;
  unsigned int i, j;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (!REGULAR_FH_P (cap->fh))
    abort ();
#endif

  args->cap = *cap;
  r = remote_md5sum (&remote_md5, args);
  if (r != ZFS_OK)
    RETURN_INT (r);

  if (remote_md5.count == 0)
    RETURN_INT (ZFS_OK);

  args->cap = *cap;
  r = local_md5sum (&local_md5, args);
  if (r != ZFS_OK)
    RETURN_INT (r);

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
      fattr fa;
      sattr sa;

      memset (&sa, -1, sizeof (sattr));
      sa.size = remote_md5.size;

      r = local_setattr (&fa, dentry, &sa, vol);
      if (r != ZFS_OK)
	RETURN_INT (r);

      r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

      dentry->fh->attr.size = fa.size;
      flush = (local_md5.size < remote_md5.size
	       && (dentry->fh->meta.flags & METADATA_COMPLETE));

      local_md5.size = remote_md5.size;
      interval_tree_delete (dentry->fh->updated, local_md5.size, UINT64_MAX);
      interval_tree_delete (dentry->fh->modified, local_md5.size, UINT64_MAX);

      if (flush || dentry->fh->updated->deleted)
	{
	  if (!flush_interval_tree (vol, dentry->fh, METADATA_TYPE_UPDATED))
	    MARK_VOLUME_DELETE (vol);
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
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  RETURN_INT (ZFS_UPDATE_FAILED);
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
	    MARK_VOLUME_DELETE (vol);
	}
    }
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* Update different blocks.  */
  for (i = 0, j = *index; i < remote_md5.count; i++)
    {
      if (remote_md5.length[i] > ZFS_MAXDATA
	  || remote_md5.offset[i] + remote_md5.length[i] > remote_md5.size)
	{
	  abort (); /* FIXME: do not abort, return error only */
	  RETURN_INT (ZFS_UPDATE_FAILED);
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
		RETURN_INT (r);

	      r = full_local_write (&count, buf, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		RETURN_INT (r);
	    }
	  else
	    {
	      /* MD5 block is larger than block(s) to be updated.  */

	      r = full_remote_read (&remote_md5.length[i], buf2, cap,
				    remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		RETURN_INT (r);

	      r = full_local_read (&count, buf, cap,
				   remote_md5.offset[i], remote_md5.length[i]);
	      if (r != ZFS_OK)
		RETURN_INT (r);

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
		RETURN_INT (r);
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
	    MARK_VOLUME_DELETE (vol);

	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	}
    }
  *index = j;

  if (flush)
    {
      r = zfs_fh_lookup (&cap->fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

      if (!flush_interval_tree (vol, dentry->fh, METADATA_TYPE_MODIFIED))
	MARK_VOLUME_DELETE (vol);

      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
    }

  RETURN_INT (ZFS_OK);
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
  unsigned int i, index;

  TRACE ("");

  if (VARRAY_USED (*blocks) == 0)
    RETURN_INT (ZFS_OK);

  args.count = 0;
  index = 0;
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
		  r = update_file_blocks_1 (&args, cap, blocks, &index);
		  if (r != ZFS_OK)
		    RETURN_INT (r);
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
      r = update_file_blocks_1 (&args, cap, blocks, &index);
      if (r != ZFS_OK)
	RETURN_INT (r);
    }

  r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  RETURN_INT (ZFS_OK);
}

/* Reintegrate modified blocks of local file CAP to remote file.  */

int32_t
reintegrate_file_blocks (zfs_cap *cap)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  interval_tree_node node;
  uint64_t offset;
  uint32_t count;
  int32_t r, r2;

  TRACE ("");

  r2 = find_capability_nolock (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  for (offset = 0; offset < dentry->fh->attr.size; )
    {
      char buf[ZFS_MAXDATA];

      CHECK_MUTEX_LOCKED (&fh_mutex);
      CHECK_MUTEX_LOCKED (&vol->mutex);
      CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

      node = interval_tree_lookup (dentry->fh->modified, offset);
      if (!node)
	break;

      if (INTERVAL_START (node) > offset)
	offset = INTERVAL_START (node);

      count = (INTERVAL_END (node) - INTERVAL_START (node) < ZFS_MAXDATA
	       ? INTERVAL_END (node) - INTERVAL_START (node) : ZFS_MAXDATA);

      r = full_local_read_dentry (&count, buf, cap, dentry, vol, offset,
				  count);
      if (r != ZFS_OK)
	break;

      if (count > 0)
	{
	  r = full_remote_write_dentry (&count, buf, cap, icap, dentry, vol,
					offset, count);
	  if (r != ZFS_OK)
	    break;
	}

      if (count > 0)
	{
	  interval_tree_delete (dentry->fh->modified, offset, offset + count);
	  offset += count;
	}
      else
	break;
    }
  zfsd_mutex_unlock (&fh_mutex);

  if (dentry->fh->modified->deleted)
    {
      if (!flush_interval_tree (vol, dentry->fh, METADATA_TYPE_MODIFIED))
	MARK_VOLUME_DELETE (vol);
    }

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  RETURN_INT (ZFS_OK);
}

/* Return true if file *DENTRYP on volume *VOLP with file handle FH should
   be updated.  Store remote attributes to ATTR.  */

static int
update_p (volume *volp, internal_dentry *dentryp, zfs_fh *fh, fattr *attr,
	  bool fh_mutex_locked)
{
  int32_t r, r2;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!((*volp)->local_path.str && (*volp)->master != this_node))
    abort ();
#endif

  if (zfs_fh_undefined ((*dentryp)->fh->meta.master_fh))
    RETURN_INT (0);

  if (fh_mutex_locked)
    zfsd_mutex_unlock (&fh_mutex);

  r = remote_getattr (attr, *dentryp, *volp);
  if (r != ZFS_OK)
    goto out;

  if (fh_mutex_locked)
    r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
  else
    r2 = zfs_fh_lookup (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if ((*dentryp)->fh->attr.type != attr->type)
    RETURN_INT (0);

  RETURN_INT (UPDATE_P (*dentryp, *attr) * IFH_UPDATE
	      + REINTEGRATE_P (*dentryp) * IFH_REINTEGRATE
	      + METADATA_CHANGE_P (*dentryp, *attr) * IFH_METADATA);

out:
  if (fh_mutex_locked)
    r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
  else
    r2 = zfs_fh_lookup (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  RETURN_INT (0);
}

/* Update file with file handle FH.  */

static int32_t
update_file (zfs_fh *fh)
{
  varray blocks;
  volume vol;
  internal_dentry dentry;
  internal_cap icap = NULL;
  zfs_cap cap;
  int32_t r, r2;
  int what;
  fattr attr;
  bool opened_remote = false;

  TRACE ("");

  r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (fh);
      if (r != ZFS_OK)
	RETURN_INT (r);
      r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
    }
  if (r != ZFS_OK)
    RETURN_INT (r);

  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node)
      || zfs_fh_undefined (dentry->fh->meta.master_fh))
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      RETURN_INT (EINVAL);
    }

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, fh);
  if (r != ZFS_OK)
    RETURN_INT (r);

  what = update_p (&vol, &dentry, fh, &attr, true);
  if (dentry->fh->attr.type != FT_REG || attr.type != FT_REG)
    {
      r = ZFS_UPDATE_FAILED;
      goto out;
    }
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  switch (what & (IFH_UPDATE | IFH_REINTEGRATE))
    {
      default:
	RETURN_INT (ZFS_OK);

      case IFH_UPDATE:
	cap.flags = O_RDONLY;
	break;

      case IFH_REINTEGRATE:
	/* File may change from other node when we are reintegrating file
	   so fallthru.  */

      case IFH_UPDATE | IFH_REINTEGRATE:
	cap.flags = O_RDWR;
	break;
    }
  cap.fh = *fh;
  r = get_capability (&cap, &icap, &vol, &dentry, NULL, false, false);
  if (r != ZFS_OK)
    goto out2;

  r = cond_remote_open (&cap, icap, &dentry, &vol);
  if (r != ZFS_OK)
    goto out2;
  opened_remote = true;

  if (!load_interval_trees (vol, dentry->fh))
    {
      MARK_VOLUME_DELETE (vol);
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      r = ZFS_METADATA_ERROR;
      goto out2;
    }

  if (what & IFH_REINTEGRATE)
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      r = reintegrate_file_blocks (&cap);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      if (r == ZFS_OK)
	what = update_p (&vol, &dentry, fh, &attr, true);
    }

  if (r == ZFS_OK && (what & IFH_UPDATE))
    {
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      get_blocks_for_updating (dentry->fh, 0, attr.size, &blocks);
      release_dentry (dentry);
      r = update_file_blocks (&cap, &blocks);
      varray_destroy (&blocks);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
    }

  if (!save_interval_trees (vol, dentry->fh))
    {
      MARK_VOLUME_DELETE (vol);
      r = ZFS_METADATA_ERROR;
      goto out;
    }

  /* If the file was not completelly updated or reintegrated
     add it to queue again.  */
  if (r == ZFS_OK
      && ((dentry->fh->meta.flags & METADATA_COMPLETE) == 0
	  || (dentry->fh->meta.flags & METADATA_MODIFIED) != 0))
    {
      zfsd_mutex_lock (&update_queue.mutex);
      queue_put (&update_queue, &dentry->fh->local_fh);
      zfsd_mutex_unlock (&update_queue.mutex);
    }
  else
    dentry->fh->flags &= ~(IFH_ENQUEUED | IFH_UPDATE | IFH_REINTEGRATE);

  goto out;

out2:
  r2 = find_capability_nolock (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

out:
  if (opened_remote)
    cond_remote_close (&cap, icap, &dentry, &vol);
  if (icap)
    put_capability (icap, dentry->fh, NULL);
  internal_dentry_unlock (vol, dentry);
  RETURN_INT (r);
}

/* Update generic file DENTRY with file handle FH on volume VOL if needed.  */

int32_t
update_fh_if_needed (volume *volp, internal_dentry *dentryp, zfs_fh *fh)
{
  int32_t r, r2;
  fattr remote_attr;
  int how;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);

  r = ZFS_OK;
  if ((*volp)->master != this_node)
    {
      how = update_p (volp, dentryp, fh, &remote_attr, true);
      if (how)
	{
	  r = update (*volp, *dentryp, fh, &remote_attr, how);

	  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
	  if (r2 != ZFS_OK)
	    RETURN_INT (r2);

	  if (r != ZFS_OK)
	    {
	      internal_dentry_unlock (*volp, *dentryp);
	      RETURN_INT (r);
	    }
	}
    }

  RETURN_INT (r);
}

/* Update generic file DENTRY on volume VOL if needed.
   DENTRY and DENTRY2 are locked before and after this macro.
   DENTRY2 might be deleted in update.  */

int32_t
update_fh_if_needed_2 (volume *volp, internal_dentry *dentryp,
		       internal_dentry *dentry2p, zfs_fh *fh, zfs_fh *fh2)
{
  int32_t r, r2;
  fattr remote_attr;
  int how;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
  CHECK_MUTEX_LOCKED (&(*dentry2p)->fh->mutex);

  r = ZFS_OK;
  if ((*volp)->master != this_node)
    {
#ifdef ENABLE_CHECKING
      if (fh->sid != fh2->sid
	  || fh->vid != fh2->vid
	  || fh->dev != fh2->dev)
	abort ();
#endif

      if (fh2->ino != fh->ino)
	release_dentry (*dentry2p);

      how = update_p (volp, dentryp, fh, &remote_attr, true);
      if (how)
	{
	  r = update (*volp, *dentryp, fh, &remote_attr, how);

	  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
	  if (r2 != ZFS_OK)
	    {
	      if (fh2->ino != fh->ino)
		{
		  r = zfs_fh_lookup_nolock (fh2, volp, dentryp, NULL, false);
		  if (r == ZFS_OK)
		    internal_dentry_unlock (*volp, *dentryp);
		}
	      RETURN_INT (r2);
	    }

	  if (r != ZFS_OK)
	    {
	      internal_dentry_unlock (*volp, *dentryp);
	      if (fh2->ino != fh->ino)
		{
		  r2 = zfs_fh_lookup_nolock (fh2, volp, dentry2p, NULL, false);
		  if (r2 == ZFS_OK)
		    internal_dentry_unlock (*volp, *dentry2p);
		}
	      RETURN_INT (r);
	    }

	  if (fh2->ino != fh->ino)
	    {
	      *dentry2p = dentry_lookup (fh2);
	      if (!*dentry2p)
		{
		  internal_dentry_unlock (*volp, *dentryp);
		  RETURN_INT (ZFS_STALE);
		}
	    }
	  else
	    *dentry2p = *dentryp;
	}
      else
	{
	  zfsd_mutex_unlock (&(*dentryp)->fh->mutex);
	  zfsd_mutex_unlock (&(*volp)->mutex);
	  zfsd_mutex_unlock (&fh_mutex);

	  r2 = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  if (fh2->ino != fh->ino)
	    {
	      *dentry2p = dentry_lookup (fh2);
#ifdef ENABLE_CHECKING
	      if (!*dentry2p)
		abort ();
#endif
	    }
	  else
	    *dentry2p = *dentryp;
	}
    }

  RETURN_INT (r);
}

/* Update generic file DENTRY on volume VOL associated with capability ICAP
   if needed.  */

int32_t
update_cap_if_needed (internal_cap *icapp, volume *volp,
		      internal_dentry *dentryp, virtual_dir *vdp,
		      zfs_cap *cap)
{
  int32_t r, r2;
  fattr remote_attr;
  zfs_fh tmp_fh;
  int how;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);

  r = ZFS_OK;
  if ((*volp)->master != this_node)
    {
      tmp_fh = (*dentryp)->fh->local_fh;
      how = update_p (volp, dentryp, &tmp_fh, &remote_attr, true);
      if (how)
	{
	  r = update (*volp, *dentryp, &tmp_fh, &remote_attr, how);

	  r2 = find_capability_nolock (cap, icapp, volp, dentryp, vdp, false);
	  if (r2 != ZFS_OK)
	    RETURN_INT (r2);

	  if (r != ZFS_OK)
	    {
	      internal_cap_unlock (*volp, *dentryp, *vdp);
	      RETURN_INT (r);
	    }

	  if (*vdp)
	    zfsd_mutex_unlock (&(*vdp)->mutex);
#ifdef ENABLE_CHECKING
	  if (!*vdp && VIRTUAL_FH_P (cap->fh))
	    abort ();
#endif
	}
    }

  RETURN_INT (r);
}

/* Delete file in place of file DENTRY on volume VOL.
   If JOURNAL_P add a journal entries to appropriate journals.  */

int32_t
delete_tree (internal_dentry dentry, volume vol, bool destroy_dentry,
	     bool journal_p, bool move_to_shadow_p)
{
  string path;
  uint32_t vid;
  int32_t r;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  build_local_path (&path, vol, dentry);
  vid = vol->id;
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = recursive_unlink (&path, vid, destroy_dentry, journal_p,
			move_to_shadow_p);
  free (path.str);

  RETURN_INT (r);
}

/* Delete file NAME in directory DIR on volume VOL.
   If JOURNAL_P add a journal entries to appropriate journals.  */

int32_t
delete_tree_name (internal_dentry dir, string *name, volume vol,
		  bool destroy_dentry, bool journal_p, bool move_to_shadow_p)
{
  string path;
  uint32_t vid;
  int32_t r;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  vid = vol->id;
  release_dentry (dir);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  r = recursive_unlink (&path, vid, destroy_dentry, journal_p,
			move_to_shadow_p);
  free (path.str);

  RETURN_INT (r);
}

/* If the local file NAME in directory DIR_FH is the same as remote file
   REMOTE_FH set SAME to true and return ZFS_OK.
   Otherwise delete NAME and its subtree from directory DIR_FH and set SAME
   to false.
   Use local attributes LOCAL_ATTR and remote attributes REMOTE_ATTR for
   comparing the files.  */

static int32_t
files_are_the_same (zfs_fh *dir_fh, string *name, fattr *local_attr,
		    zfs_fh *remote_fh, fattr *remote_attr, bool *same)
{
  internal_dentry dir;
  volume vol;
  int32_t r, r2;
  read_link_res local_link, remote_link;

  TRACE ("");

  if (local_attr->type != remote_attr->type)
    goto differ;

  switch (local_attr->type)
    {
      default:
	abort ();

      case FT_REG:
      case FT_DIR:
	break;

      case FT_LNK:
	if (local_attr->size != remote_attr->size)
	  goto differ;

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	r = local_readlink_name (&local_link, dir, name, vol);
	if (r != ZFS_OK)
	  goto differ;

	vol = volume_lookup (dir_fh->vid);
#ifdef ENABLE_CHECKING
	if (!vol)
	  abort ();
#endif

	r = remote_readlink_zfs_fh (&remote_link, remote_fh, vol);
	if (r != ZFS_OK)
	  {
	    free (local_link.path.str);
	    goto differ;
	  }

	if (local_link.path.len != remote_link.path.len
	    || memcmp (local_link.path.str, remote_link.path.str,
		       local_link.path.len) != 0)
	  {
	    free (local_link.path.str);
	    free (remote_link.path.str);
	    goto differ;
	  }
	free (local_link.path.str);
	free (remote_link.path.str);

	break;

      case FT_BLK:
      case FT_CHR:
	if (local_attr->rdev != remote_attr->rdev)
	  goto differ;
	break;

      case FT_SOCK:
      case FT_FIFO:
	break;
    }

  *same = true;
  RETURN_INT (ZFS_OK);

differ:
  *same = false;
  RETURN_INT (ZFS_OK);
}

/* Set mode, UID and GID in metadata META for file FH on volume VOL
   according to ATTR.  */

static void
set_metadata_attr (volume vol, metadata *meta, zfs_fh *fh, fattr *attr)
{
  internal_dentry dentry;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  dentry = dentry_lookup (fh);
  if (dentry)
    *meta = dentry->fh->meta;

  meta->flags = METADATA_COMPLETE;
  meta->modetype = GET_MODETYPE (attr->mode, attr->type);
  meta->uid = attr->uid;
  meta->gid = attr->gid;

  if (!flush_metadata (vol, meta))
    MARK_VOLUME_DELETE (vol);

  if (dentry)
    {
      dentry->fh->meta.flags = meta->flags;
      dentry->fh->meta.modetype = meta->modetype;
      dentry->fh->meta.uid = meta->uid;
      dentry->fh->meta.gid = meta->gid;
      release_dentry (dentry);
    }
}

/* Synchronize attributes of local file LOCAL_FH with attributes LOCAL_ATTR
   and attributes of remote file REMOTE_FH with attributes REMOTE_ATTR.
   LOCAL_CHANGED and REMOTE_CHANGED specify which attributes have changed.  */

static int32_t
synchronize_attributes (volume *volp, internal_dentry *dentryp,
			zfs_fh *fh, fattr *attr,
			bool local_changed, bool remote_changed)
{
  metadata meta;
  fattr fa;
  sattr sa;
  int32_t r = ZFS_OK;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);

#ifdef ENABLE_CHECKING
  if (! (local_changed ^ remote_changed))
    abort ();
#endif

  sa.size = (uint64_t) -1;
  sa.atime = (zfs_time) -1;
  sa.mtime = (zfs_time) -1;
  if ((*dentryp)->fh->level == LEVEL_UNLOCKED)
    meta = (*dentryp)->fh->meta;

  if (local_changed)
    {
      sa.mode = (*dentryp)->fh->attr.mode;
      sa.uid = (*dentryp)->fh->attr.uid;
      sa.gid = (*dentryp)->fh->attr.gid;

      zfsd_mutex_unlock (&fh_mutex);
      r = remote_setattr (attr, *dentryp, &sa, *volp);
    }
  if (remote_changed)
    {
      sa.mode = attr->mode;
      sa.uid = attr->uid;
      sa.gid = attr->gid;

      r = local_setattr (&fa, *dentryp, &sa, *volp);
    }

  if (r != ZFS_OK)
    RETURN_INT (r);

  r = zfs_fh_lookup_nolock (fh, volp, dentryp, NULL, false);
  if (r == ZFS_OK)
    {
      if (remote_changed)
	(*dentryp)->fh->attr = fa;

      (*dentryp)->fh->meta.modetype = GET_MODETYPE ((*dentryp)->fh->attr.mode,
						    (*dentryp)->fh->attr.type);
      (*dentryp)->fh->meta.uid = (*dentryp)->fh->attr.uid;
      (*dentryp)->fh->meta.gid = (*dentryp)->fh->attr.gid;
      if (!flush_metadata (*volp, &(*dentryp)->fh->meta))
	MARK_VOLUME_DELETE (*volp);
    }
  else
    {
      *dentryp = NULL;
      *volp = volume_lookup (fh->vid);
#ifdef ENABLE_CHECKING
      /* Dentry or its parent is locked.  */
      if (!*volp)
	abort ();
#endif

      meta.flags = METADATA_COMPLETE;
      meta.modetype = GET_MODETYPE (sa.mode, attr->type);
      meta.uid = sa.uid;
      meta.gid = sa.gid;
      if (!flush_metadata (*volp, &meta))
	MARK_VOLUME_DELETE (*volp);

      zfsd_mutex_unlock (&(*volp)->mutex);
    }

  RETURN_INT (ZFS_OK);
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

  TRACE ("");
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
			  O_CREAT | O_WRONLY | O_TRUNC, &sa, vol, &meta, NULL);
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
	  RETURN_INT (r);

	r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	sa.mode = (uint32_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;
	r = local_symlink (&res, dir, name, &link_to.path, &sa, vol, &meta);
	free (link_to.path.str);
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
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      dentry = get_dentry (local_fh, remote_fh, vol, dir, name,
			   local_attr, &meta);
      release_dentry (dir);
      zfsd_mutex_unlock (&fh_mutex);

      if (dentry->fh->attr.type == FT_REG)
	flags = dentry->fh->meta.flags & ~METADATA_COMPLETE;
      else if (dentry->fh->attr.type == FT_DIR)
	flags = 0;
      else
	flags = METADATA_COMPLETE;

      ok = set_metadata (vol, dentry->fh, flags,
			 remote_attr->version, remote_attr->version);
      release_dentry (dentry);
      if (!ok)
	{
	  MARK_VOLUME_DELETE (vol);
	  r = ZFS_METADATA_ERROR;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }

  RETURN_INT (r);
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

  TRACE ("");
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
	  RETURN_INT (r);

	r2 = zfs_fh_lookup (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
	  abort ();
#endif

	sa.mode = (uint32_t) -1;
	sa.atime = (zfs_time) -1;
	sa.mtime = (zfs_time) -1;
	r = remote_symlink (res, dir, name, &link_to.path, &sa, vol);
	free (link_to.path.str);
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

  RETURN_INT (r);
}

/* Synchronize file DENTRY with file handle FH on volume VOL
   with the remote file with attributes ATTR.
   HOW describe what needs to be done.  */

static int32_t
synchronize_file (volume vol, internal_dentry dentry, zfs_fh *fh, fattr *attr,
		  int how)
{
  internal_dentry parent, conflict, dentry2;
  bool local_changed, remote_changed;
  bool want_conflict;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node))
    abort ();
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
  if (dentry->fh->level == LEVEL_UNLOCKED)
    abort ();
#endif

  local_changed = METADATA_ATTR_CHANGE_P (dentry->fh->meta, dentry->fh->attr);
  remote_changed = METADATA_ATTR_CHANGE_P (dentry->fh->meta, *attr);

#ifdef ENABLE_CHECKING
  if (how & IFH_METADATA)
    {
      if (!local_changed && !remote_changed)
	abort ();
    }
  else
    {
      if (local_changed || remote_changed)
	abort ();
    }
#endif

  if (local_changed ^ remote_changed)
    {
      int32_t r;

      r = synchronize_attributes (&vol, &dentry, fh, attr, local_changed,
				  remote_changed);
      if (r != ZFS_OK)
	RETURN_INT (r);
    }

  if (dentry->fh->attr.type == FT_REG)
    {
      if (dentry->fh->attr.version == dentry->fh->meta.master_version
	  || attr->version == dentry->fh->meta.master_version)
	{
	  /* Schedule update or reintegration of regular file.  */

	  zfsd_mutex_lock (&running_mutex);
	  if (update_pool.main_thread == 0)
	    {
	      /* Update threads are not running.  */
	      zfsd_mutex_unlock (&running_mutex);
	    }
	  else
	    {
	      zfsd_mutex_unlock (&running_mutex);

	      if (!(dentry->fh->flags & IFH_ENQUEUED))
		{
		  dentry->fh->flags |= IFH_ENQUEUED;
		  zfsd_mutex_lock (&update_queue.mutex);
		  queue_put (&update_queue, &dentry->fh->local_fh);
		  zfsd_mutex_unlock (&update_queue.mutex);
		}
	    }
	}
    }

  want_conflict = ((dentry->fh->attr.version > dentry->fh->meta.master_version
		    && attr->version > dentry->fh->meta.master_version)
		   || (local_changed && remote_changed));

  conflict = dentry->parent;
  if (conflict)
    acquire_dentry (conflict);
  if (conflict && CONFLICT_DIR_P (conflict->fh->local_fh))
    {
      if (want_conflict)
	{
	  release_dentry (dentry);
	  dentry2 = conflict_remote_dentry (conflict);
	  if (dentry2)
	    {
	      dentry2->fh->attr = *attr;
	      release_dentry (dentry2);
	    }
	  release_dentry (conflict);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	}
      else
	{
	  release_dentry (dentry);
	  cancel_conflict (vol, conflict);
	}
    }
  else
    {
      parent = conflict;
      if (want_conflict)
	{
	  string name;

	  /* Create a modify-modify or an attr-attr conflict.  */
	  xstringdup (&name, &dentry->name);
	  release_dentry (dentry);
	  conflict = create_conflict (vol, parent, &name, fh,
				      &dentry->fh->attr);
	  free (name.str);
	  add_file_to_conflict_dir (vol, conflict, true,
				    &dentry->fh->meta.master_fh, attr, NULL);
	  release_dentry (conflict);
	}
      else
	release_dentry (dentry);

      if (parent)
	release_dentry (parent);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }

  RETURN_INT (ZFS_OK);
}

/* Discard changes to local file LOCAL which is in conflict with REMOTE
   on volume VOL.  CONFLICT_FH is a file handle of the cnflict directory
   containing these two files.  */

int32_t
resolve_conflict_discard_local (zfs_fh *conflict_fh, internal_dentry local,
				internal_dentry remote, volume vol)
{
  internal_dentry conflict;
  sattr sa;
  fattr fa;
  int32_t r, r2;
  uint64_t version;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&local->fh->mutex);
  CHECK_MUTEX_LOCKED (&remote->fh->mutex);

  /* Synchronize the attributes if necessary.  */
  if (METADATA_ATTR_CHANGE_P (local->fh->meta, local->fh->attr)
      && METADATA_ATTR_CHANGE_P (local->fh->meta, remote->fh->attr))
    {
      sa.mode = (local->fh->attr.mode != remote->fh->attr.mode
		 ? remote->fh->attr.mode : (uint32_t) -1);
      sa.uid = (local->fh->attr.uid != remote->fh->attr.uid
		? remote->fh->attr.uid : (uint32_t) -1);
      sa.gid = (local->fh->attr.gid != remote->fh->attr.gid
		? remote->fh->attr.gid : (uint32_t) -1);
      sa.size = (uint64_t) -1;
      sa.atime = (zfs_time) -1;
      sa.mtime = (zfs_time) -1;
      release_dentry (remote);
      r = local_setattr (&fa, local, &sa, vol);
      if (r != ZFS_OK)
	RETURN_INT (r);

      r2 = zfs_fh_lookup_nolock (conflict_fh, &vol, &conflict, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
      local = conflict_local_dentry (conflict);
      remote = conflict_other_dentry (conflict, local);
      release_dentry (conflict);
#ifdef ENABLE_CHECKING
      if (!local)
	abort ();
      if (!remote)
	abort ();
#endif

      set_attr_version (&fa, &local->fh->meta);
      local->fh->attr = fa;
      local->fh->meta.modetype = GET_MODETYPE (fa.mode, fa.type);
      local->fh->meta.uid = fa.uid;
      local->fh->meta.gid = fa.gid;
      if (!flush_metadata (vol, &local->fh->meta))
	MARK_VOLUME_DELETE (vol);
    }
  zfsd_mutex_unlock (&fh_mutex);

  version = (local->fh->attr.version > remote->fh->attr.version
	     ? local->fh->attr.version + 1 : remote->fh->attr.version + 1);
  release_dentry (remote);

  /* Update the interval trees.  */
  if (!load_interval_trees (vol, local->fh))
    goto out;

  interval_tree_sub (local->fh->updated, local->fh->modified);
  interval_tree_empty (local->fh->modified);
  if (local->fh->interval_tree_users > 1)
    {
      if (!flush_interval_tree (vol, local->fh, METADATA_TYPE_UPDATED))
	goto out_save;

      if (!flush_interval_tree (vol, local->fh, METADATA_TYPE_MODIFIED))
	goto out_save;
    }

  if (!save_interval_trees (vol, local->fh))
    goto out;

  /* Update local and remote version.  */
  r = local_reintegrate_set (local, version, vol);
  if (r != ZFS_OK)
    RETURN_INT (r);

  r2 = zfs_fh_lookup_nolock (conflict_fh, &vol, &conflict, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif
  remote = conflict_remote_dentry (conflict);
  release_dentry (conflict);
  zfsd_mutex_unlock (&fh_mutex);

  remote->fh->attr.version = version;
  r = remote_reintegrate_set (remote, version, NULL, vol);
  RETURN_INT (r);

out_save:
  save_interval_trees (vol, local->fh);

out:
  release_dentry (local);
  zfsd_mutex_unlock (&vol->mutex);
  RETURN_INT (ZFS_METADATA_ERROR);
}

/* Discard changes to local file LOCAL which is in conflict with REMOTE
   on volume VOL.  CONFLICT_FH is a file handle of the cnflict directory
   containing these two files.  */

int32_t
resolve_conflict_discard_remote (zfs_fh *conflict_fh, internal_dentry local,
				 internal_dentry remote, volume vol)
{
  internal_dentry conflict;
  sattr sa;
  fattr fa;
  int32_t r, r2;
  uint64_t version;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&local->fh->mutex);
  CHECK_MUTEX_LOCKED (&remote->fh->mutex);

  /* Synchronize the attributes if necessary.  */
  if (METADATA_ATTR_CHANGE_P (local->fh->meta, local->fh->attr)
      && METADATA_ATTR_CHANGE_P (local->fh->meta, remote->fh->attr))
    {
      sa.mode = (local->fh->attr.mode != remote->fh->attr.mode
		 ? local->fh->attr.mode : (uint32_t) -1);
      sa.uid = (local->fh->attr.uid != remote->fh->attr.uid
		? local->fh->attr.uid : (uint32_t) -1);
      sa.gid = (local->fh->attr.gid != remote->fh->attr.gid
		? local->fh->attr.gid : (uint32_t) -1);
      sa.size = (uint64_t) -1;
      sa.atime = (zfs_time) -1;
      sa.mtime = (zfs_time) -1;
      release_dentry (remote);
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_setattr (&fa, local, &sa, vol);
      if (r != ZFS_OK)
	RETURN_INT (r);

      r2 = zfs_fh_lookup_nolock (conflict_fh, &vol, &conflict, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
      local = conflict_local_dentry (conflict);
      remote = conflict_other_dentry (conflict, local);
      release_dentry (conflict);
#ifdef ENABLE_CHECKING
      if (!local)
	abort ();
      if (!remote)
	abort ();
#endif

      remote->fh->attr = fa;
      local->fh->meta.modetype = GET_MODETYPE (fa.mode, fa.type);
      local->fh->meta.uid = fa.uid;
      local->fh->meta.gid = fa.gid;
      if (!flush_metadata (vol, &local->fh->meta))
	MARK_VOLUME_DELETE (vol);
    }
  zfsd_mutex_unlock (&fh_mutex);

  version = (local->fh->attr.version > remote->fh->attr.version
	     ? local->fh->attr.version : remote->fh->attr.version + 1);
  release_dentry (remote);

  /* Update the interval trees.  */
  if (!load_interval_trees (vol, local->fh))
    goto out;

  interval_tree_add (local->fh->modified, local->fh->updated);
  interval_tree_empty (local->fh->updated);
  if (local->fh->interval_tree_users > 1)
    {
      if (!flush_interval_tree (vol, local->fh, METADATA_TYPE_UPDATED))
	goto out_save;

      if (!flush_interval_tree (vol, local->fh, METADATA_TYPE_MODIFIED))
	goto out_save;
    }

  if (!save_interval_trees (vol, local->fh))
    goto out;

  /* Update local and remote version.  */
  r = local_reintegrate_set (local, version + 1, vol);
  if (r != ZFS_OK)
    RETURN_INT (r);

  r2 = zfs_fh_lookup_nolock (conflict_fh, &vol, &conflict, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif
  remote = conflict_remote_dentry (conflict);
  release_dentry (conflict);
  zfsd_mutex_unlock (&fh_mutex);

  remote->fh->attr.version = version;
  r = remote_reintegrate_set (remote, version, NULL, vol);
  RETURN_INT (r);

out_save:
  save_interval_trees (vol, local->fh);

out:
  release_dentry (local);
  zfsd_mutex_unlock (&vol->mutex);
  RETURN_INT (ZFS_METADATA_ERROR);
}

/* Resolve conflict by deleting local file NAME with local file handle LOCAL_FH
   and remote file handle REMOTE_FH in directory DIR with file handle DIR_FH
   on volume VOL.  Store the info about deleted file into RES.  */

int32_t
resolve_conflict_delete_local (dir_op_res *res, internal_dentry dir,
			       zfs_fh *dir_fh, string *name, zfs_fh *local_fh,
			       zfs_fh *remote_fh, volume vol)
{
  file_info_res info;
  metadata meta;
  int32_t r, r2;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  r = local_lookup (res, dir, name, vol, &meta);
  if (r != ZFS_OK)
    RETURN_INT (r);

  if (!ZFS_FH_EQ (res->file, *local_fh))
    RETURN_INT (ENOENT);

  if (!zfs_fh_undefined (*remote_fh))
    {
      vol = volume_lookup (remote_fh->vid);
#ifdef ENABLE_CHECKING
      if (!vol)
	abort ();
#endif
      r = remote_file_info (&info, remote_fh, vol);
      if (r == ZFS_OK)
	free (info.path.str);
    }
  else
    r = ENOENT;

  if (r == ZFS_OK)
    {
      /* Remote file exists.  */

      RETURN_INT (local_reintegrate_del_base (&res->file, name, false, dir_fh,
					      true));
    }
  else if (r == ENOENT || r == ESTALE)
    {
      /* Remote file does not exist.  */

      r2 = zfs_fh_lookup_nolock (dir_fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      if (delete_tree_name (dir, name, vol, false, true, true) != ZFS_OK)
	RETURN_INT (ZFS_UPDATE_FAILED);
      RETURN_INT (ZFS_OK);
    }
  else
    {
      message (0, stderr,
	       "Resolve: file info error: %d (%s)\n", r, zfs_strerror (r));
    }

  RETURN_INT (r);
}

/* Resolve conflict by deleting remote file NAME with file handle REMOTE_FH
   in directory DIR on volume VOL.  */

int32_t
resolve_conflict_delete_remote (volume vol, internal_dentry dir, string *name,
				zfs_fh *remote_fh)
{
  fh_mapping map;
  zfs_fh dir_fh;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (*remote_fh))
    abort ();
#endif

  if (!get_fh_mapping_for_master_fh (vol, remote_fh, &map))
    {
      MARK_VOLUME_DELETE (vol);
      release_dentry (dir);
      zfsd_mutex_unlock (&vol->mutex);
      RETURN_INT (ZFS_METADATA_ERROR);
    }

  dir_fh = dir->fh->local_fh;
  RETURN_INT (remote_reintegrate_del (vol, remote_fh, dir, name,
				      map.slot_status != VALID_SLOT, &dir_fh));
}

/* Update the directory DIR on volume VOL with file handle FH,
   set attributes according to ATTR.  */

static int32_t
update_dir (volume vol, internal_dentry dir, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;
  internal_dentry conflict;
  filldir_htab_entries local_entries, remote_entries;
  dir_op_res local_res, remote_res;
  metadata meta;
  dir_entry *entry;
  void **slot, **slot2;
  file_info_res info;
  fh_mapping map;
  bool want_conflict;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dir->fh) && vol->master != this_node))
    abort ();
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
  if (dir->fh->attr.type != FT_DIR)
    abort ();
  if (dir->fh->level == LEVEL_UNLOCKED)
    abort ();
#endif

  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = full_local_readdir (fh, &local_entries);
  if (r != ZFS_OK)
    RETURN_INT (r);

  r = full_remote_readdir (fh, &remote_entries);
  if (r != ZFS_OK)
    {
      htab_destroy (local_entries.htab);
      RETURN_INT (r);
    }

  HTAB_FOR_EACH_SLOT (local_entries.htab, slot)
    {
      entry = (dir_entry *) *slot;

      if (LOCAL_VOLUME_ROOT_P (dir)
	  && strcmp (entry->name.str, ".shadow") == 0)
	{
	  htab_clear_slot (local_entries.htab, slot);
	  continue;
	}

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      r = local_lookup (&local_res, dir, &entry->name, vol, &meta);
      if (r == ENOENT)
	{
	  /* The local file was moved or deleted while updating other
	     entries.  */
	  htab_clear_slot (local_entries.htab, slot);
	  continue;
	}
      if (r != ZFS_OK)
	goto out;

      slot2 = htab_find_slot (remote_entries.htab, entry, NO_INSERT);
      if (slot2)
	{
	  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  r = remote_lookup (&remote_res, dir, &entry->name, vol);
	  if (r != ZFS_OK)
	    goto out;

	  want_conflict = true;
	  if (ZFS_FH_EQ (meta.master_fh, remote_res.file))
	    {
	      bool same;
	      bool local_changed, remote_changed;

	      r = files_are_the_same (fh, &entry->name, &local_res.attr,
				      &remote_res.file, &remote_res.attr,
				      &same);
	      if (r != ZFS_OK)
		goto out;

	      if (!same)
		{
		  /* If the special files have the same file handle
		     but do not have same contents
		     delete the local file because this could have happened
		     if master has deleted metadata and the new file
		     got the same file handle.  */

		  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
		  if (r2 != ZFS_OK)
		    abort ();
#endif

		  r = delete_tree_name (dir, &entry->name, vol, true, false,
					true);
		  if (r != ZFS_OK)
		    goto out;

		  htab_clear_slot (local_entries.htab, slot);
		  continue;
		}

	      local_changed = METADATA_ATTR_CHANGE_P (meta, local_res.attr);
	      if (local_changed
		  && METADATA_ATTR_EQ_P (local_res.attr, remote_res.attr))
		{
		  set_metadata_attr (vol, &meta, &local_res.file,
				     &local_res.attr);
		  local_changed = false;
		}
	      remote_changed = METADATA_ATTR_CHANGE_P (meta, remote_res.attr);
	      want_conflict = local_changed && remote_changed;

	      if (local_changed ^ remote_changed)
		{
		  internal_dentry dentry;

		  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
		  if (r2 != ZFS_OK)
		    abort ();
#endif
		  dentry = get_dentry (&local_res.file, &remote_res.file,
				       vol, dir, &entry->name, &local_res.attr,
				       &meta);
		  release_dentry (dir);

		  r = synchronize_attributes (&vol, &dentry, &local_res.file,
					      &remote_res.attr, local_changed,
					      remote_changed);
		  if (r != ZFS_OK)
		    goto out;

		  if (dentry)
		    {
		      release_dentry (dentry);
		      zfsd_mutex_unlock (&vol->mutex);
		      zfsd_mutex_unlock (&fh_mutex);
		    }
		}
	    }

	  if (want_conflict)
	    {
	      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	      if (r2 != ZFS_OK)
		abort ();
#endif

	      /* Create a create-create or an attr-attr conflict.  */
	      conflict = create_conflict (vol, dir, &entry->name,
					  &local_res.file, &local_res.attr);
	      add_file_to_conflict_dir (vol, conflict, true, &local_res.file,
					&local_res.attr, &meta);
	      add_file_to_conflict_dir (vol, conflict, true, &remote_res.file,
					&remote_res.attr, NULL);
	      release_dentry (conflict);
	      release_dentry (dir);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);
	    }
	  htab_clear_slot (local_entries.htab, slot);
	  htab_clear_slot (remote_entries.htab, slot2);
	  continue;
	}

      if (!zfs_fh_undefined (meta.master_fh)
	  && (meta.flags & METADATA_MODIFIED) == 0)
	{
	  vol = volume_lookup (fh->vid);
#ifdef ENABLE_CHECKING
	  if (!vol)
	    abort ();
#endif

	  r = remote_file_info (&info, &meta.master_fh, vol);
	  if (r == ZFS_OK)
	    free (info.path.str);

	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  r = local_reintegrate_del (vol, &local_res.file, dir, &entry->name,
				     r != ZFS_OK, fh, false);
	}
      else if (meta.flags & METADATA_MODIFIED)
	{
	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  /* Create a modify-delete conflict.  */
	  remote_res.file.sid = dir->fh->meta.master_fh.sid;
	  conflict = create_conflict (vol, dir, &entry->name, &local_res.file,
				      &local_res.attr);
	  add_file_to_conflict_dir (vol, conflict, true, &local_res.file,
				    &local_res.attr, &meta);
	  add_file_to_conflict_dir (vol, conflict, false, &remote_res.file,
				    &local_res.attr, NULL);
	  release_dentry (conflict);
	  release_dentry (dir);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	}
      else
	{
	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  r = delete_tree_name (dir, &entry->name, vol, true, false, true);
	  if (r != ZFS_OK)
	    goto out;
	}

      htab_clear_slot (local_entries.htab, slot);
    }

  HTAB_FOR_EACH_SLOT (remote_entries.htab, slot)
    {
      entry = (dir_entry *) *slot;

      r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      r = remote_lookup (&remote_res, dir, &entry->name, vol);
      if (r == ENOENT || r == ESTALE)
	{
	  htab_clear_slot (remote_entries.htab, slot);
	  continue;
	}
      if (r != ZFS_OK)
	goto out;

      vol = volume_lookup (fh->vid);
#ifdef ENABLE_CHECKING
      if (!vol)
	abort ();
#endif

      if (!get_fh_mapping_for_master_fh (vol, &remote_res.file, &map))
	{
	  MARK_VOLUME_DELETE (vol);
	  zfsd_mutex_unlock (&vol->mutex);
	  r = ZFS_METADATA_ERROR;
	  goto out;
	}
      zfsd_mutex_unlock (&vol->mutex);

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      if (map.slot_status == VALID_SLOT)
	{
	  r = local_reintegrate_add (vol, dir, &entry->name, &map.local_fh,
				     fh, false);
	  if (r != ZFS_OK)
	    goto out;
	}
      else
	{
	  r = create_local_fh (dir, &entry->name, vol, fh,
			       &remote_res.file, &remote_res.attr);
	  if (r != ZFS_OK)
	    goto out;
	}

      htab_clear_slot (remote_entries.htab, slot);
    }

  r = ZFS_OK;
out:
  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (!dir->fh->journal->first)
    {
      if (!set_metadata (vol, dir->fh, r == ZFS_OK ? METADATA_COMPLETE : 0,
			 attr->version, attr->version))
	MARK_VOLUME_DELETE (vol);
    }

  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  htab_destroy (local_entries.htab);
  htab_destroy (remote_entries.htab);
  RETURN_INT (r);
}

/* Reintegrate journal of deleted directory DIR_ENTRY on volume VID.
   Use RES for lookups.  */

static int32_t
reintegrate_deleted_dir (dir_op_res *res, uint32_t vid, journal_entry dir_entry)
{
  file_info_res info;
  zfs_fh file_fh;
  zfs_fh fh;
  volume vol;
  journal_t journal;
  journal_entry entry, next;
  int32_t r;
  bool flush_journal;
  bool defined_master_fh;
  bool local_exists;

  TRACE ("");

  fh.dev = dir_entry->dev;
  fh.ino = dir_entry->ino;
  fh.gen = dir_entry->gen;
  journal = journal_create (10, NULL);

  vol = volume_lookup (vid);
#ifdef ENABLE_CHECKING
  if (!vol)
    abort ();
#endif
  if (!read_journal (vol, &fh, journal))
    {
      journal_destroy (journal);
      MARK_VOLUME_DELETE (vol);
      zfsd_mutex_unlock (&vol->mutex);
      RETURN_INT (ZFS_OK);
    }
  zfsd_mutex_unlock (&vol->mutex);

  defined_master_fh = !zfs_fh_undefined (dir_entry->master_fh);
  flush_journal = false;
  for (entry = journal->first; entry; entry = next)
    {
      next = entry->next;

      switch (entry->oper)
	{
	  case JOURNAL_OPERATION_ADD:
	    if (!journal_delete_entry (journal, entry))
	      abort ();
	    flush_journal = true;
	    break;

	  case JOURNAL_OPERATION_DEL:
	    /* Process subtree if possible.  */
	    vol = volume_lookup (vid);
#ifdef ENABLE_CHECKING
	    if (!vol)
	      abort ();
#endif
	    file_fh.dev = entry->dev;
	    file_fh.ino = entry->ino;
	    file_fh.gen = entry->gen;
	    r = local_file_info (&info, &file_fh, vol);
	    zfsd_mutex_unlock (&vol->mutex);
	    local_exists = (r == ZFS_OK);
	    if (r == ZFS_OK)
	      free (info.path.str);
	    else
	      {
		r = reintegrate_deleted_dir (res, vid, entry);
		if (r != ZFS_OK)
		  goto out;
	      }

	    if (defined_master_fh)
	      {
		vol = volume_lookup (vid);
#ifdef ENABLE_CHECKING
		if (!vol)
		  abort ();
#endif
		r = remote_lookup_zfs_fh (res, &dir_entry->master_fh,
					  &entry->name, vol);
	      }
	    else
	      r = ENOENT;

	    if (r == ZFS_OK)
	      {
		if (ZFS_FH_EQ (res->file, entry->master_fh))
		  {
		    bool destroy;

		    vol = volume_lookup (vid);
#ifdef ENABLE_CHECKING
		    if (!vol)
		      abort ();
#endif
		    destroy = (!local_exists
			       && entry->master_version == res->attr.version);
		    r = remote_reintegrate_del_zfs_fh (vol, &entry->master_fh,
						       &dir_entry->master_fh,
						       &entry->name, destroy);
		    if (r == ZFS_OK)
		      {
			if (!journal_delete_entry (journal, entry))
			  abort ();
			flush_journal = true;
		      }
		    else if (r != ENOENT && r != ESTALE)
		      goto out;
		  }
		else
		  {
		    /* There is another file with NAME so the original file
		       must have been already deleted.  */
		    if (!journal_delete_entry (journal, entry))
		      abort ();
		    flush_journal = true;
		  }
	      }
	    else if (r == ENOENT || r == ESTALE)
	      {
		/* Nothing to do.  */

		if (!journal_delete_entry (journal, entry))
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
  r = ZFS_OK;

out:
  if (flush_journal)
    {
      vol = volume_lookup (vid);
#ifdef ENABLE_CHECKING
      if (!vol)
	abort ();
#endif
      if (!write_journal (vol, &fh, journal))
	MARK_VOLUME_DELETE (vol);
      zfsd_mutex_unlock (&vol->mutex);
    }

  journal_destroy (journal);
  RETURN_INT (r);
}

/* Reintegrate journal for directory DIR on volume VOL with file handle FH.
   Update version of remote directrory in ATTR.  */

static int32_t
reintegrate_dir (volume vol, internal_dentry dir, zfs_fh *fh, fattr *attr)
{
  int32_t r, r2;
  internal_dentry conflict;
  journal_entry entry, next;
  dir_op_res local_res;
  dir_op_res res;
  metadata meta;
  zfs_fh file_fh;
  file_info_res info;
  bool flush_journal;
  bool local_volume_root;
  bool local_exists;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dir->fh) && vol->master != this_node))
    abort ();
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
  if (dir->fh->attr.type != FT_DIR)
    abort ();
  if (dir->fh->level == LEVEL_UNLOCKED)
    abort ();
#endif

  local_volume_root = LOCAL_VOLUME_ROOT_P (dir);
  flush_journal = false;
  for (entry = dir->fh->journal->first; entry; entry = next)
    {
      next = entry->next;

      CHECK_MUTEX_LOCKED (&fh_mutex);
      CHECK_MUTEX_LOCKED (&vol->mutex);
      CHECK_MUTEX_LOCKED (&dir->fh->mutex);

      if (local_volume_root && SPECIAL_NAME_P (entry->name.str, true))
	{
	  if (!journal_delete_entry (dir->fh->journal, entry))
	    abort ();
	  flush_journal = true;
	  continue;
	}

      switch (entry->oper)
	{
	  case JOURNAL_OPERATION_ADD:
	    /* Check whether local file still exists.  */
	    r = local_lookup (&local_res, dir, &entry->name, vol, &meta);
	    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	    if (r2 != ZFS_OK)
	      abort ();
#endif
	    if (r != ZFS_OK)
	      {
		if (!journal_delete_entry (dir->fh->journal, entry))
		  abort ();
		flush_journal = true;
		continue;
	      }

	    zfsd_mutex_unlock (&fh_mutex);
	    r = remote_lookup (&res, dir, &entry->name, vol);
	    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	    if (r2 != ZFS_OK)
	      abort ();
#endif

	    if (r == ZFS_OK)
	      {
		if (!ZFS_FH_EQ (meta.master_fh, res.file))
		  {
		    /* Create a create-create conflict.  */
		    conflict = create_conflict (vol, dir, &entry->name,
						&local_res.file,
						&local_res.attr);
		    add_file_to_conflict_dir (vol, conflict, true,
					      &local_res.file,
					      &local_res.attr, &meta);
		    add_file_to_conflict_dir (vol, conflict, true,
					      &res.file, &res.attr, NULL);
		    release_dentry (conflict);
		  }
		else
		  {
		    if (!journal_delete_entry (dir->fh->journal, entry))
		      abort ();
		    flush_journal = true;
		  }
	      }
	    else if (r == ENOENT || r == ESTALE)
	      {
		if (zfs_fh_undefined (meta.master_fh))
		  {
		    internal_dentry subdentry;
		    bool success;

		    r = create_remote_fh (&res, dir, &entry->name, vol,
					  fh, &local_res.attr);
		    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
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

		    success = flush_metadata (vol, &meta);

		    if (subdentry)
		      {
			if (success)
			  {
			    subdentry->fh->meta = meta;
			    set_attr_version (&subdentry->fh->attr,
					      &subdentry->fh->meta);
			  }
			release_dentry (subdentry);
		      }

		    if (!success)
		      {
			MARK_VOLUME_DELETE (vol);
			continue;
		      }

		    release_dentry (dir);
		    zfsd_mutex_unlock (&fh_mutex);

		    /* Set remote metadata. */
		    r = remote_reintegrate_set (NULL, meta.master_version,
						&res.file, vol);

		    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
		    if (r2 != ZFS_OK)
		      abort ();
#endif
		    if (r != ZFS_OK)
		      continue;

		    if (!journal_delete_entry (dir->fh->journal, entry))
		      abort ();
		    flush_journal = true;
		  }
		else
		  {
		    release_dentry (dir);
		    zfsd_mutex_unlock (&fh_mutex);
		    r = remote_file_info (&info, &entry->master_fh, vol);
		    if (r == ZFS_OK)
		      free (info.path.str);

		    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
		    if (r2 != ZFS_OK)
		      abort ();
#endif

		    if (r == ZFS_OK)
		      {
			zfsd_mutex_unlock (&fh_mutex);
			r = remote_reintegrate_add (vol, dir,
						    &entry->name,
						    &entry->master_fh, fh);
			r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r == ZFS_OK)
			  {
			    if (!journal_delete_entry (dir->fh->journal,
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
			r = delete_tree_name (dir, &entry->name, vol, true,
					      true, true);
			r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r != ZFS_OK)
			  goto out;

			if (!journal_delete_entry (dir->fh->journal, entry))
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

	    file_fh.dev = entry->dev;
	    file_fh.ino = entry->ino;
	    file_fh.gen = entry->gen;
	    r = local_file_info (&info, &file_fh, vol);
	    local_exists = (r == ZFS_OK);
	    if (r == ZFS_OK)
	      free (info.path.str);

	    r = remote_lookup (&res, dir, &entry->name, vol);

	    if (!local_exists)
	      {
		r2 = reintegrate_deleted_dir (&local_res, fh->vid, entry);
		if (r2 != ZFS_OK)
		  goto out;
	      }

	    r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	    if (r2 != ZFS_OK)
	      abort ();
#endif

	    if (r == ZFS_OK)
	      {
		if (ZFS_FH_EQ (res.file, entry->master_fh))
		  {
		    if (!local_exists
			&& res.attr.type == FT_REG
			&& entry->master_version != res.attr.version)
		      {
			/* File does not exist on local and was modified
			   on master.  Create a delete-modify conflict.  */
			local_res.file.sid = this_node->id;
			local_res.file.vid = vol->id;
			local_res.file.dev = entry->dev;
			local_res.file.ino = entry->ino;
			local_res.file.gen = entry->gen;
			conflict = create_conflict (vol, dir, &entry->name,
						    &local_res.file,
						    &res.attr);
			add_file_to_conflict_dir (vol, conflict, true,
						  &res.file, &res.attr, NULL);
			add_file_to_conflict_dir (vol, conflict, false,
						  &local_res.file, &res.attr,
						  NULL);
			release_dentry (conflict);
		      }
		    else
		      {
			zfsd_mutex_unlock (&fh_mutex);
			r = remote_reintegrate_del (vol, &entry->master_fh,
						    dir, &entry->name,
						    !local_exists, fh);
			r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL,
						   false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
			  abort ();
#endif
			if (r == ZFS_OK)
			  {
			    if (!journal_delete_entry (dir->fh->journal,
						       entry))
			      abort ();
			    flush_journal = true;
			  }
		      }
		  }
		else
		  {
		    /* There is another file with NAME so the original file
		       must have been already deleted.  */
		    if (!journal_delete_entry (dir->fh->journal, entry))
		      abort ();
		    flush_journal = true;
		  }
	      }
	    else if (r == ENOENT || r == ESTALE)
	      {
		/* Nothing to do.  */

		if (!journal_delete_entry (dir->fh->journal, entry))
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

  if (!dir->fh->journal->first && flush_journal)
    {
      uint64_t version;
      unsigned long delay, range;

      /* If the journal is empty set the local and remote version.  */
      
      zfsd_mutex_unlock (&fh_mutex);

      range = 40000;
      do
	{
	  r = remote_reintegrate (dir, 1, vol);
	  if (r == ZFS_BUSY)
	    {
	      delay = range / 4 + RANDOM (range);
	      range += 40000;
	      usleep (range);
	    }

	  r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif
	}
      while (r == ZFS_BUSY);
      if (r != ZFS_OK)
	goto out2;

      r = remote_getattr (attr, dir, vol);
      r2 = zfs_fh_lookup (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
      if (r != ZFS_OK)
	{
	  /* This could happen only if there is a problem with connection.
	     In this case, the master will allow other node to start
	     reintegration so there is no need to send a release request.  */
	  goto out2;
	}

      if (!lookup_metadata (vol, &dir->fh->local_fh, &dir->fh->meta, true))
	{
	  MARK_VOLUME_DELETE (vol);
	  version = attr->version;
	}
      else
	{
	  if (dir->fh->meta.local_version > attr->version)
	    version = dir->fh->meta.local_version;
	  else
	    version = attr->version;

	  dir->fh->meta.flags &= ~METADATA_COMPLETE;
	  dir->fh->meta.local_version = version;
	  dir->fh->meta.master_version = version;
	  if (!flush_metadata (vol, &dir->fh->meta))
	    MARK_VOLUME_DELETE (vol);
	}

      /* Wee need to call following call even if VERSION == ATTR->VERSION
	 because we need to release the right to reintegrate the dir.  */
      r = remote_reintegrate_set (dir, version, NULL, vol);
      if (r == ZFS_OK)
	attr->version = version;

      r2 = zfs_fh_lookup_nolock (fh, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
    }
out:
  zfsd_mutex_unlock (&fh_mutex);
out2:
  if (flush_journal)
    {
      if (!write_journal (vol, &dir->fh->local_fh, dir->fh->journal))
	MARK_VOLUME_DELETE (vol);
    }

  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);

  RETURN_INT (ZFS_OK);
}

/* Reintegrate or update generic file DENTRY on volume VOL with file handle FH
   and remote file attributes ATTR.  HOW specifies what we should do.  */

int32_t
update (volume vol, internal_dentry dentry, zfs_fh *fh, fattr *attr, int how)
{
  int32_t r;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node))
    abort ();
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  switch (dentry->fh->attr.type)
    {
      default:
	abort ();

      case FT_REG:
	r = synchronize_file (vol, dentry, fh, attr, how);
	break;

      case FT_DIR:
	r = synchronize_file (vol, dentry, fh, attr, how);
	if (r != ZFS_OK)
	  RETURN_INT (r);

	r = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
	if (r != ZFS_OK)
	  RETURN_INT (r);

	r = reintegrate_dir (vol, dentry, fh, attr);
	if (r != ZFS_OK)
	  RETURN_INT (r);

	r = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, false);
	if (r != ZFS_OK)
	  RETURN_INT (r);

	r = update_dir (vol, dentry, fh, attr);
	break;

      case FT_LNK:
      case FT_BLK:
      case FT_CHR:
      case FT_SOCK:
      case FT_FIFO:
	r = synchronize_file (vol, dentry, fh, attr, how);
	break;
    }

  RETURN_INT (r);
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

      t->from_sid = this_node->id;
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

  if (!thread_pool_create (&update_pool, 6, 2, 4, update_main,
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
