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

/* Are metadata (mode, UID and GID) different in META and ATTR?  */
#define METADATA_ATTR_CHANGE_P(META, ATTR)				\
  ((ATTR).mode != GET_MODETYPE_MODE ((META).modetype)			\
   || (ATTR).uid != (META).uid						\
   || (ATTR).gid != (META).gid)

/* Have local or remote metadata (mode, UID and GID) changed?  */
#define METADATA_CHANGE_P(DENTRY, ATTR)					\
  (METADATA_ATTR_CHANGE_P ((DENTRY)->fh->meta, (DENTRY)->fh->attr)	\
   || METADATA_ATTR_CHANGE_P ((DENTRY)->fh->meta, ATTR))

/* Are metadata in attributes ATTR1 and ATTR2 equal?  */
#define METADATA_ATTR_EQ_P(ATTR1, ATTR2)				\
  ((ATTR1).mode == (ATTR2).mode						\
   && (ATTR1).uid == (ATTR2).uid					\
   && (ATTR1).gid == (ATTR2).gid)

/* Queue of file handles.  */
extern queue update_queue;

/* Pool of update threads.  */
extern thread_pool update_pool;

extern void get_blocks_for_updating (internal_fh fh, uint64_t start,
				     uint64_t end, varray *blocks);
extern int32_t update_file_blocks (zfs_cap *cap, varray *blocks);
extern int32_t reintegrate_file_blocks (zfs_cap *cap);
extern int32_t update_fh_if_needed (volume *volp, internal_dentry *dentryp,
				    zfs_fh *fh);
extern int32_t update_fh_if_needed_2 (volume *volp, internal_dentry *dentryp,
				      internal_dentry *dentry2p, zfs_fh *fh,
				      zfs_fh *fh2);
extern int32_t update_cap_if_needed (internal_cap *icapp, volume *volp,
				     internal_dentry *dentryp,
				     virtual_dir *vdp, zfs_cap *cap);
extern int update_p (volume *volp, internal_dentry *dentryp, zfs_fh *fh,
		     fattr *attr);
extern int32_t delete_tree (internal_dentry dentry, volume vol,
			    bool destroy_dentry);
extern int32_t delete_tree_name (internal_dentry dir, string *name, volume vol,
				 bool destroy_dentry);
extern int32_t resolve_conflict_discard_local (zfs_fh *conflict_fh,
					       internal_dentry local,
					       internal_dentry remote,
					       volume vol);
extern int32_t resolve_conflict_discard_remote (zfs_fh *conflict_fh,
						internal_dentry local,
						internal_dentry remote,
						volume vol);
extern int32_t resolve_conflict_delete_local (dir_op_res *res,
					      internal_dentry dir,
					      zfs_fh *dir_fh, string *name,
					      zfs_fh *local_fh,
					      zfs_fh *remote_fh, volume vol);
extern int32_t resolve_conflict_delete_remote (volume vol, internal_dentry dir,
					       string *name, zfs_fh *remote_fh);
extern int32_t update (volume vol, internal_dentry dentry, zfs_fh *fh,
		       fattr *attr, int how);

extern bool update_start (void);
extern void update_cleanup (void);

#endif
