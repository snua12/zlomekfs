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

#ifndef UPDATE_H
#define UPDATE_H

#include "system.h"
#include <inttypes.h>
#include "pthread.h"
#include "varray.h"
#include "queue.h"
#include "thread.h"
#include "fh.h"
#include "cap.h"
#include "metadata.h"
#include "zfs_prot.h"

#define ZFS_UPDATED_BLOCK_SIZE ZFS_MAXDATA
#define ZFS_MODIFIED_BLOCK_SIZE 1024

/* Update the file DENTRY according to remote attributes ATTR
   if local file was not modified and
   remote file was modified since we updated it last time
   OR the file has not been completelly updated
   and local or remote file was not modified (i.e. handle
   partially updated file).  */
#define UPDATE_P(DENTRY, ATTR)						    \
  (((DENTRY)->fh->attr.version == (DENTRY)->fh->meta.master_version	    \
    && (ATTR).version > (DENTRY)->fh->meta.master_version)		    \
   || (!((DENTRY)->fh->meta.flags & METADATA_COMPLETE)			    \
       && ((DENTRY)->fh->attr.version == (DENTRY)->fh->meta.master_version  \
	   || (ATTR).version == (DENTRY)->fh->meta.master_version)))

/* Reintegrate the file DENTRY
   if local file was modified since we reintegrated it last time.
   If both UPDATE_P and REINTEGRATE_P are true, there is a conflict
   on the file.  */
#define REINTEGRATE_P(DENTRY)						\
  ((DENTRY)->fh->attr.version > (DENTRY)->fh->meta.master_version)

/* Update generic file DENTRY on volume VOL if needed.  */
#define UPDATE_FH_IF_NEEDED(VOL, DENTRY, FH)				\
  do {									\
    fattr remote_attr;							\
    int how;								\
									\
    if ((VOL)->master != this_node)					\
      {									\
	how = update_p (&(VOL), &(DENTRY), &(FH), &remote_attr);	\
	if (how)							\
	  {								\
	    r = update_fh ((VOL), (DENTRY), &(FH), &remote_attr, how);	\
									\
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL,	\
				       false);				\
	    if (r2 != ZFS_OK)						\
	      return r2;						\
									\
	    if (r != ZFS_OK)						\
	      {								\
		internal_dentry_unlock ((VOL), (DENTRY));		\
		return r;						\
	      }								\
	  }								\
      }									\
  } while (0)

/* Update generic file DENTRY on volume VOL if needed.
   DENTRY and DENTRY2 are locked before and after this macro.
   DENTRY2 might be deleted in update_fh.  */
#define UPDATE_FH_IF_NEEDED_2(VOL, DENTRY, DENTRY2, FH, FH2)		\
  do {									\
    fattr remote_attr;							\
    int how;								\
									\
    if ((VOL)->master != this_node)					\
      {									\
	if (ENABLE_CHECKING_VALUE					\
	    && (GET_SID (FH) != GET_SID (FH2)				\
		|| (FH).vid != (FH2).vid				\
		|| (FH).dev != (FH2).dev))				\
	  abort ();							\
									\
	if ((FH2).ino != (FH).ino)					\
	  release_dentry ((DENTRY2));					\
									\
	how = update_p (&(VOL), &(DENTRY), &(FH), &remote_attr);	\
	if (how)							\
	  {								\
	    r = update_fh ((VOL), (DENTRY), &(FH), &remote_attr, how);	\
									\
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL,	\
				       false);				\
	    if (r2 != ZFS_OK)						\
	      {								\
		if ((FH2).ino != (FH).ino)				\
		  {							\
		    r = zfs_fh_lookup_nolock (&(FH2), &(VOL), &(DENTRY),\
					      NULL, false);		\
		    if (r == ZFS_OK)					\
		      internal_dentry_unlock ((VOL), (DENTRY));		\
		  }							\
		return r2;						\
	      }								\
									\
	    if (r != ZFS_OK)						\
	      {								\
		internal_dentry_unlock ((VOL), (DENTRY));		\
		if ((FH2).ino != (FH).ino)				\
		  {							\
		    r2 = zfs_fh_lookup_nolock (&(FH2), &(VOL),		\
					       &(DENTRY2), NULL, false);\
		    if (r2 == ZFS_OK)					\
		      internal_dentry_unlock ((VOL), (DENTRY2));	\
		  }							\
		return r;						\
	      }								\
									\
	    if ((FH2).ino != (FH).ino)					\
	      {								\
		(DENTRY2) = dentry_lookup (&(FH2));			\
		if (!(DENTRY2))						\
		  {							\
		    internal_dentry_unlock ((VOL), (DENTRY));		\
		    return ZFS_STALE;					\
		  }							\
	      }								\
	    else							\
	      (DENTRY2) = (DENTRY);					\
	  }								\
	else								\
	  {								\
	    zfsd_mutex_unlock (&(DENTRY)->fh->mutex);			\
	    zfsd_mutex_unlock (&(VOL)->mutex);				\
	    zfsd_mutex_unlock (&fh_mutex);				\
									\
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL,	\
				       false);				\
	    if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)			\
	      abort ();							\
									\
	    if ((FH2).ino != (FH).ino)					\
	      {								\
		(DENTRY2) = dentry_lookup (&(FH2));			\
		if (ENABLE_CHECKING_VALUE && !(DENTRY2))		\
		  abort ();						\
	      }								\
	    else							\
	      (DENTRY2) = (DENTRY);					\
          }								\
      }									\
  } while (0)

/* Update generic file DENTRY on volume VOL associated with capability ICAP
   if needed.  */
#define UPDATE_CAP_IF_NEEDED(ICAP, VOL, DENTRY, VD, CAP)		\
  do {									\
    fattr remote_attr;							\
    zfs_fh tmp_fh;							\
    int how;								\
									\
    if ((VOL)->master != this_node)					\
      {									\
	tmp_fh = (DENTRY)->fh->local_fh;				\
	how = update_p (&(VOL), &(DENTRY), &tmp_fh, &remote_attr);	\
	if (how)							\
	  {								\
	    r = update_fh ((VOL), (DENTRY), &tmp_fh, &remote_attr, how);\
									\
	    if (VIRTUAL_FH_P ((CAP).fh))				\
	      zfsd_mutex_lock (&vd_mutex);				\
	    r2 = find_capability_nolock (&(CAP), &(ICAP), &(VOL),	\
					&(DENTRY), &(VD), false);	\
	    if (r2 != ZFS_OK)						\
	      {								\
		if (VIRTUAL_FH_P ((CAP).fh))				\
		  zfsd_mutex_unlock (&vd_mutex);			\
		return r2;						\
	      }								\
									\
	    if (r != ZFS_OK)						\
	      {								\
		internal_cap_unlock ((VOL), (DENTRY), (VD));		\
		return r;						\
	      }								\
									\
	    if (VD)							\
	      {								\
		zfsd_mutex_unlock (&(VD)->mutex);			\
		zfsd_mutex_unlock (&vd_mutex);				\
	      }								\
	    if (ENABLE_CHECKING_VALUE					\
		&& !(VD) && VIRTUAL_FH_P ((CAP).fh))			\
	      abort ();							\
	  }								\
      }									\
  } while (0)

/* Queue of file handles.  */
extern queue update_queue;

/* Pool of update threads.  */
extern thread_pool update_pool;

extern void get_blocks_for_updating (internal_fh fh, uint64_t start,
				     uint64_t end, varray *blocks);
extern int32_t update_file_blocks (zfs_cap *cap, varray *blocks);
extern int update_p (volume *volp, internal_dentry *dentryp, zfs_fh *fh,
		     fattr *attr);
extern int32_t update_fh (volume vol, internal_dentry dentry, zfs_fh *fh,
			  fattr *attr, int how);

extern bool update_start (void);
extern void update_cleanup (void);

#endif
