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

#ifndef UPDATE_H
#define UPDATE_H

#include "system.h"
#include <inttypes.h>
#include "varray.h"
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

/* Update generic file DENTRY on volume VOL if needed.  */
#define UPDATE_FH_IF_NEEDED(VOL, DENTRY, FH)				\
  do {									\
    fattr remote_attr;							\
									\
    if ((VOL)->master != this_node)					\
      {									\
	if (update_p (&(DENTRY), &(VOL), &(FH), &remote_attr))		\
	  {								\
	    zfsd_mutex_unlock (&fh_mutex);				\
									\
	    r = update_fh ((DENTRY), (VOL), &(FH), &remote_attr);	\
									\
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL);	\
	    if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)			\
	      abort ();							\
									\
	    if (r != ZFS_OK)						\
	      {								\
		internal_dentry_unlock ((DENTRY));			\
		zfsd_mutex_unlock (&(VOL)->mutex);			\
		zfsd_mutex_unlock (&fh_mutex);				\
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
									\
    if ((VOL)->master != this_node)					\
      {									\
	if (ENABLE_CHECKING_VALUE					\
	    && ((FH).sid != (FH2).sid					\
		|| (FH).vid != (FH2).vid				\
		|| (FH).dev != (FH2).dev))				\
	  abort ();							\
									\
	if ((FH2).ino != (FH).ino)					\
	  release_dentry ((DENTRY2));					\
									\
	if (update_p (&(DENTRY), &(VOL), &(FH), &remote_attr))		\
	  {								\
	    zfsd_mutex_unlock (&fh_mutex);				\
									\
	    r = update_fh ((DENTRY), (VOL), &(FH), &remote_attr);	\
	    if (r != ZFS_OK)						\
	      return r;							\
									\
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL);	\
	    if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)			\
	      abort ();							\
									\
	    if (r != ZFS_OK)						\
	      {								\
		internal_dentry_unlock ((DENTRY));			\
		zfsd_mutex_unlock (&(VOL)->mutex);			\
		zfsd_mutex_unlock (&fh_mutex);				\
		return r;						\
	      }								\
									\
	    if ((FH2).ino != (FH).ino)					\
	      {								\
		(DENTRY2) = dentry_lookup (&(FH2));			\
		if (!(DENTRY2))						\
		  {							\
		    internal_dentry_unlock ((DENTRY));			\
		    zfsd_mutex_unlock (&(VOL)->mutex);			\
		    zfsd_mutex_unlock (&fh_mutex);			\
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
	    r2 = zfs_fh_lookup_nolock (&(FH), &(VOL), &(DENTRY), NULL);	\
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
									\
    if ((VOL)->master != this_node)					\
      {									\
	tmp_fh = (DENTRY)->fh->local_fh;				\
	if (update_p (&(DENTRY), &(VOL), &tmp_fh, &remote_attr))	\
	  {								\
	    zfsd_mutex_unlock (&fh_mutex);				\
									\
	    r = update_fh ((DENTRY), (VOL), &tmp_fh, &remote_attr);	\
	    if (r != ZFS_OK)						\
	      return r;							\
									\
	    if (VIRTUAL_FH_P ((CAP).fh))				\
	      zfsd_mutex_lock (&vd_mutex);				\
	    r = find_capability_nolock (&(CAP), &(ICAP), &(VOL), &(DENTRY),\
				 &(VD));				\
	    if (VIRTUAL_FH_P ((CAP).fh))				\
	      zfsd_mutex_unlock (&vd_mutex);				\
	    if (ENABLE_CHECKING_VALUE && r != ZFS_OK)			\
	      abort ();							\
	    if (VD)							\
	      zfsd_mutex_unlock (&(VD)->mutex);				\
	  }								\
      }									\
  } while (0)

extern void get_blocks_for_updating (internal_fh fh, uint64_t start,
				     uint64_t end, varray *blocks);
extern int32_t update_file_blocks (bool use_buffer, uint32_t *rcount,
				   void *buffer, uint64_t offset,
				   internal_cap cap, varray *blocks);
extern bool update_p (internal_dentry *dentryp, volume *volp, zfs_fh *fh,
		      fattr *attr);
extern int32_t update_fh (internal_dentry dentry, volume vol, zfs_fh *fh,
			  fattr *attr);

#endif
