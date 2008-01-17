/*! \file
    \brief Functions for updating files.  */

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
#include "zfs-prot.h"

/*! \brief Maximum block size for updating
 *
 * \see ZFS_MAXDATA
 *
 */
#define ZFS_UPDATED_BLOCK_SIZE ZFS_MAXDATA

/*! \brief Maximum block size for reintegrating */
#define ZFS_MODIFIED_BLOCK_SIZE 1024

/*! \brief Check whether we should update a generic file.
 *
 * Update the generic file if it has not been completelly updated yet,
 * otherwise update a directory if the remote version has changed since
 * the last time we updated the directory
 * or update a regular file if local file was not modified and remote file was
 * modified since we updated it last time.
 *
 *  \param DENTRY The dentry of the file to be checked.
 *  \param ATTR The remote attributes of the file.
 *
 */
#define UPDATE_P(DENTRY, ATTR)						   \
  (!((DENTRY)->fh->meta.flags & METADATA_COMPLETE)			   \
   || ((DENTRY)->fh->attr.type == FT_DIR				   \
       ? (ATTR).version > (DENTRY)->fh->meta.master_version		   \
       : (((DENTRY)->fh->attr.version == (DENTRY)->fh->meta.master_version \
           && (ATTR).version > (DENTRY)->fh->meta.master_version))))

/*! \brief Check whether we should reintegrate a generic file.
 *
 * Reintegrate a directory if the local version has changed since the last
 * time we reintegrated the directory or it was not completely reintegrated.
 * Reintegrate a regular file if remote file was not modified and local file
 * was modified since we reintegrated it last time or it was not completely
 * reintegrated.
 *
 *  \param DENTRY The dentry of the file to be checked.
 *  \param ATTR The remote attributes of the file.
 *
 */
#define REINTEGRATE_P(DENTRY, ATTR)					\
  ((DENTRY)->fh->attr.type == FT_DIR					\
   ? (DENTRY)->fh->attr.version > (DENTRY)->fh->meta.master_version	\
   : ((ATTR).version == (DENTRY)->fh->meta.master_version		\
      && (DENTRY)->fh->attr.version > (DENTRY)->fh->meta.master_version))

/*! \brief Are file sizes (for regular files) different? */
#define METADATA_SIZE_CHANGE_P(ATTR1, ATTR2)			\
        (((ATTR1).type == FT_REG) && ((ATTR1).size != (ATTR2).size))

/*! \brief Did the master version (for regular files) change? */
#define METADATA_MASTER_VERSION_CHANGE_P(DENTRY, ATTR)                        \
        (((ATTR).type == FT_REG) && ((DENTRY)->fh->meta.master_version != (ATTR).version))

/*! \brief Are metadata (mode, UID and GID) different in META and ATTR? */
#define METADATA_ATTR_CHANGE_P(META, ATTR)				\
  ((ATTR).mode != GET_MODETYPE_MODE ((META).modetype)			\
   || (ATTR).uid != (META).uid						\
   || (ATTR).gid != (META).gid)

/*! \brief Have local or remote metadata/attributes (mode, UID and GID, size and master version) changed? */
#define METADATA_CHANGE_P(DENTRY, ATTR)					\
  (METADATA_ATTR_CHANGE_P ((DENTRY)->fh->meta, (DENTRY)->fh->attr)	\
   || METADATA_ATTR_CHANGE_P ((DENTRY)->fh->meta, ATTR)			\
   || METADATA_SIZE_CHANGE_P ((DENTRY)->fh->attr, ATTR)                 \
   || METADATA_MASTER_VERSION_CHANGE_P(DENTRY, ATTR))

/*! \brief Are metadata/attributes (more, uid, guid, size) in attributes ATTR1 and ATTR2 equal? */
#define METADATA_ATTR_EQ_P(ATTR1, ATTR2)				\
  ((ATTR1).mode == (ATTR2).mode						\
   && (ATTR1).uid == (ATTR2).uid					\
   && (ATTR1).gid == (ATTR2).gid                                        \
   && !METADATA_SIZE_CHANGE_P(ATTR1, ATTR2))

/* Queue of file handles for updating or reintegrating.  */
extern queue update_queue;

/* Pool of update threads.  */
extern thread_pool update_pool;

extern void get_blocks_for_updating (internal_fh fh, uint64_t start,
                                     uint64_t end, varray *blocks);
extern int32_t update_file_blocks (zfs_cap *cap, varray *blocks,
                                   bool modified, bool slow);
extern int32_t update_fh_if_needed (volume *volp, internal_dentry *dentryp,
                                    zfs_fh *fh, int what);
extern int32_t update_fh_if_needed_2 (volume *volp, internal_dentry *dentryp,
                                      internal_dentry *dentry2p, zfs_fh *fh,
                                      zfs_fh *fh2, int what);
extern int32_t update_cap_if_needed (internal_cap *icapp, volume *volp,
                                     internal_dentry *dentryp,
                                     virtual_dir *vdp, zfs_cap *cap,
                                     bool put_cap, int what);
extern int32_t delete_tree (internal_dentry dentry, volume vol,
                            bool destroy_dentry, bool journal_p,
                            bool move_to_shadow_p);
extern int32_t delete_tree_name (internal_dentry dir, string *name, volume vol,
                                 bool destroy_dentry, bool journal_p,
                                 bool move_to_shadow_p);
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

/** \page file-updating File updating and reintegration in ZFS.
 *
 * <h2>Introduction</h2>
 *
 * File updating and reintegration (or synchronization in general) means
 * propagating the changes between local cached file and the same file on volume
 * master (remote node). These changes are:
 * <ul>
 * <li>file attributes and metadata (mode, uid, gid, size and master version) - synchronized by #synchronize_attributes()
 * <li>data in file changed on remote node that need fetching to local node (update) - handled by #update() and #update_file(), which call #update_file_blocks()
 * <li>data in file changed on local node that need to be sent to remote node (reintegration) - handled also by #update() and #update_file(), which call #reintegrate_file_blocks()
 * </ul>
 * This synchronization can be done synchronously, for example when opening directory by #zfs_open(),
 * everything gets synchronized, when opening file by #zfs_open(), only metadata is synchronized.
 * File contents of regular files can be scheduled via schedule_update_or_reintegration() and then
 * updated and reintegrated on background by threads in #update_pool via #update_file() or synchronously,
 * when user reads/writes them, via #update(). The background updating/reintegration is different for
 * volumes, whose volume master is node connected via fast connection, and for volume masters with slow
 * connection. This is determined by measuring latency when connecting to the volume master.
 *
 * <h2>Changes in background updating/reintegration for slowly connected volumes</h2>
 *
 * Formerly, there were no background operations allowed for such volumes. The reason was to prevent
 * these operations from congesting the node's connection, thus slowing down other, more interactive
 * operations, like listing and walking through directories. But sometimes user doesn't need the connectivity
 * for anything else, and it would be nice to get new/changed files integrated to remote master, or fully updated
 * in local cache for future use. So the background synchronisation is now enabled for slow volumes too.
 * But it's important to monitor connection usage and hold the background operations in favour of the interactive
 * ones. The solution used here presumes that all slowly connected volumes are slow because of one common
 * bottleneck (for example user connected via GPRS on his notebook has slow connection to every remote volume).
 * Thus, the monitored value is total number of requests being sent/waiting for or getting response on
 * all slow connected volumes, stored and managed by #pending_slow_reqs_count and its condvar and mutex. The background
 * integration for slow volumes is done by one special thread from the update pool, which pauses its operation when it
 * detects this number being above zero. This favours interactive operations on slow connected
 * volumes, but it doesn't prevent ZFS from slowing down other applications's connection. Because it would be
 * difficult to determine when ZFS could use the line or not, it's up to user to shape ZFS's bandwith
 * for his needs, for example by ZFS's default listening port.
 *
 * <h2>Other changes/bugfixes to file synchronisation</h2>
 *
 * The first change is about file sizes. Previously, each new physical file created (locally or remotely)
 * by synchronising directory content, had size of zero bytes, regardless of the opposite side's size.
 * Then the size grew as the file was being updated or reintegrated, until it was fully done. Subsequent
 * changes to file size also weren't visible for the other side until actual updating/reintegration of data.
 * This was inconvenient for stat() operations performed on such files, because they didn't reflect size
 * that the user should really see. And after mmap() support was added, it wasn't even possible to read data from such
 * file, because reads using kernel page cache don't try to read more data if they see file size being lower (zero).
 * The solution is that during file creation (create_local_fh() and create_remote_fh()) and attributes
 * synchronisation (synchronize_attributes()), opposite side's file size is
 * used to ftruncate() the local underlying physical file, shrinking or preallocating it (with zeroes). The kernel
 * module can then see the proper size of files, and so can the user.
 *
 * The second change (or bugfix) is about the synchronize_file() function. It gets called in zfs_open() to synchronize with
 * remote file, so the user gets actual version of the file. But the effects of master version being increased weren't
 * dealt properly here, and under certain (race) conditions the newer version on volume master was ignored and old local
 * version was being read without a way to invoke updating. Now there's a update_file_clear_updated_tree_1() call in such
 * situation, which fixes it. The schedule_update_or_reintegration() call was also moved to the end of this function, when the
 * metadata are already synchronized and no conflict was created. As a small optimisation, zfs_open() doesn't request
 * regular file background reintegration no more, only updating, because local changes should be reintegrated after file gets
 * closed, while having local data updated before we need them is always convenient.
 */
