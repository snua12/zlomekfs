/* File handle functions.
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

#ifndef FH_H
#define FH_H

/* Forward declaration.  */
typedef struct volume_def *volume;

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include "pthread.h"
#include "memory.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "varray.h"
#include "fibheap.h"
#include "interval.h"
#include "journal.h"
#include "zfs_prot.h"
#include "util.h"

#define VIRTUAL_DEVICE 1	/* Device number of device with virtual
				   directories */
#define ROOT_INODE 1		/* Inode number of the root dir of ZFS */

/* Maximal number of file handles locked by one thread.  */
#define MAX_LOCKED_FILE_HANDLES 2

/* Is the FH virtual?  */
#define VIRTUAL_FH_P(FH) ((FH).vid == VOLUME_ID_VIRTUAL			\
			  && (FH).sid == NODE_NONE)

/* Is FH a file handle of non-existing file represented as a symlink
   to existing file in case of exist-non_exist conflict?  */
#define NON_EXIST_FH_P(FH) ((FH).vid == VOLUME_ID_VIRTUAL		\
			    && (FH).sid != NODE_NONE)

/* Is FH a conflict directroy?  */
#define CONFLICT_DIR_P(FH) ((FH).sid == NODE_NONE			\
			    && (FH).vid != VOLUME_ID_VIRTUAL)

/* Is FH a regular file handle, i.e. not special file handle?  */
#define REGULAR_FH_P(FH) ((FH).sid != NODE_NONE				\
			  && (FH).vid != VOLUME_ID_VIRTUAL)

/* Is DENTRY a local volume root?  */
#define LOCAL_VOLUME_ROOT_P(DENTRY)					\
  ((DENTRY)->parent == NULL						\
   || ((DENTRY)->parent->parent == NULL					\
       && CONFLICT_DIR_P ((DENTRY)->parent->fh->local_fh)))

/* True when the NAME would be a special dir if the NAME was in local
   volume root.  If ALWAYS is true return true even if request came from local
   node, otherwise return true only if request came from remote node.  */
#define SPECIAL_NAME_P(NAME, ALWAYS)					\
  (strcmp ((NAME), ".zfs") == 0						\
   || (strcmp ((NAME), ".shadow") == 0					\
       && ((ALWAYS) || !request_from_this_node ())))

/* Is file NAME in directory DIR a special dir?  If ALWAYS is true return true
   even if request came from local node, otherwise return true only if
   request came from remote node.  */
#define SPECIAL_DIR_P(DIR, NAME, ALWAYS)				\
  (LOCAL_VOLUME_ROOT_P (DIR) && SPECIAL_NAME_P ((NAME), (ALWAYS)))

/* Mark the ZFS file handle FH to be undefined.  */
#define zfs_fh_undefine(FH) (sizeof (FH) == sizeof (zfs_fh)		\
			     ? memset (&(FH), -1, sizeof (zfs_fh))	\
			     : (abort (), (void *) 0))

/* Return true if the ZFS file handle FH is undefined.  */
#define zfs_fh_undefined(FH) (bytecmp (&(FH), -1, sizeof (zfs_fh)))

/* Hash function for zfs_fh FH.  */
#define ZFS_FH_HASH(FH) (crc32_buffer ((FH), sizeof (zfs_fh)))

/* Return true if FH1 and FH2 are the same.  */
#define ZFS_FH_EQ(FH1, FH2) ((FH1).ino == (FH2).ino			\
			     && (FH1).dev == (FH2).dev			\
			     && (FH1).vid == (FH2).vid			\
			     && (FH1).sid == (FH2).sid			\
			     && (FH1).gen == (FH2).gen)

/* Hash function for internal dentry D, computed from fh->local_fh.  */
#define INTERNAL_DENTRY_HASH(D)						\
  (ZFS_FH_HASH (&(D)->fh->local_fh))

/* Hash function for internal dentry D, computed from parent->fh and name.  */
#define INTERNAL_DENTRY_HASH_NAME(D)					\
  (crc32_update (crc32_buffer ((D)->name.str, (D)->name.len),		\
		 &(D)->parent->fh->local_fh, sizeof (zfs_fh)))

/* True if file handle FH has a local path.  */
#define INTERNAL_FH_HAS_LOCAL_PATH(FH)					\
  ((FH)->local_fh.sid == this_node->id					\
   && (FH)->local_fh.vid != VOLUME_ID_VIRTUAL)

/* "Lock" level of the file handle or virtual directory.  */
#define LEVEL_UNLOCKED	0
#define LEVEL_SHARED	1
#define LEVEL_EXCLUSIVE	2

/* Flags for internal_dentry_create_conflict.  */
#define CONFLICT_LOCAL_EXISTS	1
#define CONFLICT_REMOTE_EXISTS	2
#define CONFLICT_BOTH_EXIST	(CONFLICT_LOCAL_EXISTS | CONFLICT_REMOTE_EXISTS)

/* Forward definitions.  */
typedef struct internal_fh_def *internal_fh;
typedef struct internal_dentry_def *internal_dentry;
typedef struct virtual_dir_def *virtual_dir;

#include "volume.h"
#include "cap.h"
#include "metadata.h"

/* Internal information about file handle.  */
struct internal_fh_def
{
#ifdef ENABLE_CHECKING
  long unused0;
  long unused1;
#endif

  pthread_mutex_t mutex;
  pthread_cond_t cond;

  /* File handle for client, key for hash table.  */
  zfs_fh local_fh;

  /* Number of directory entries associated with this file handle.  */
  unsigned int ndentries;

  /* File attributes.  */
  fattr attr;

  /* Metadata.  */
  metadata meta;

  /* Contained directory entries (of type 'struct internal_dentry_def *').  */
  varray subdentries;

  /* Chain of capabilities associated with this file handle.  */
  internal_cap cap;

  /* Updated intervals.  */
  interval_tree updated;

  /* Modified intervals.  */
  interval_tree modified;

  /* Journal for a directory.  */
  journal_t journal;

  /* Number of users of interval trees.  */
  unsigned int interval_tree_users;

  /* "Lock" level of the file handle.  */
  unsigned int level;

  /* Number of current users of the file handle.  */
  unsigned int users;

  /* Open file descriptor.  */
  int fd;

  /* Generation of open file descriptor.  */
  unsigned int generation;

  /* Flags, see IFH_* below.  */
  unsigned int flags;
};

/* Internal file handle flags.  */
#define IFH_UPDATE	1
#define IFH_REINTEGRATE	2
#define IFH_METADATA	4
#define IFH_ENQUEUED	8

/* Information about locked file handle.  */
typedef struct lock_info_def
{
  internal_fh fh;
  unsigned int level;
} lock_info;

/* Internal directory entry.  */
struct internal_dentry_def
{
  /* Mutex is not needed here because we can use FH->MUTEX
     because FH is constant for each internal dentry.  */

  /* Internal file handle associated with this dentry.  */
  internal_fh fh;

  /* Pointer to internal dentry of the parent directory.  */
  internal_dentry parent;

  /* File name.  */
  string name;

  /* Pointers to next and previous dentry with the same file handle,
     making a cyclic double linked chain.  */
  internal_dentry next, prev;

  /* Index of this dentry in parent's list of directory entries.  */
  unsigned int dentry_index;

  /* Last use of this dentry.  */
  time_t last_use;

  /* Heap node whose data is this dentry.  */
  fibnode heap_node;

  /* Is dentry marked to be deleted?  */
  bool deleted;
};

/* Structure of a virtual directory (element of mount tree).  */
struct virtual_dir_def
{
#ifdef ENABLE_CHECKING
  long unused0;
  long unused1;
#endif

  pthread_mutex_t mutex;

  /* Handle of this node.  */
  zfs_fh fh;

  /* Pointer to parent virtual directory.  */
  virtual_dir parent;

  /* Directory name.  */
  string name;

  /* Volume which is mounted here.  */
  volume vol;

  /* Capability associated with this virtual directory.  */
  internal_cap cap;

  /* Directory attributes.  */
  fattr attr;

  /* Virtual subdirectories (of type 'struct virtual_dir_def *').  */
  varray subdirs;

  /* Index in parent's list of subdirectories.  */
  unsigned int subdir_index;

  /* Total number of mountpoints in subtree (including current node).  */
  unsigned int n_mountpoints;

  /* Is the virtual directory busy?  If it is it can't be deleted.  */
  bool busy;

  /* Number of current users of the virtual directory.  */
  unsigned int users;

  /* Number of mountpoints to be deleted.  */
  unsigned int deleted;
};

/* File handle of ZFS root.  */
extern zfs_fh root_fh;

/* Static undefined ZFS file handle.  */
extern zfs_fh undefined_fh;

/* Hash table of used dentries, searched by fh->local_fh.  */
extern htab_t dentry_htab;

/* Hash table of virtual directories, searched by fh.  */
extern htab_t vd_htab;

/* Mutes for file handles, dentries and virtual directories.  */
extern pthread_mutex_t fh_mutex;

/* Thread ID of thread freeing dentries unused for a long time.  */
extern pthread_t cleanup_dentry_thread;

/* This mutex is locked when cleanup dentry thread is in sleep.  */
extern pthread_mutex_t cleanup_dentry_thread_in_syscall;

extern void set_lock_info (lock_info *li);
extern void set_owned (internal_fh fh, unsigned int level);
extern int32_t zfs_fh_lookup (zfs_fh *fh, volume *volp,
			      internal_dentry *dentryp, virtual_dir *vdp,
			      bool delete_volume_p);
extern int32_t zfs_fh_lookup_nolock (zfs_fh *fh, volume *volp,
				     internal_dentry *dentryp,
				     virtual_dir *vdp, bool delete_volume_p);
extern void acquire_dentry (internal_dentry dentry);
extern void release_dentry (internal_dentry dentry);
extern internal_dentry get_dentry (zfs_fh *local_fh, zfs_fh *master_fh,
				   volume vol, internal_dentry dir,
				   string *name, fattr *attr, metadata *meta);
extern void delete_dentry (volume *volp, internal_dentry *dirp, string *name,
			   zfs_fh *dir_fh);
extern virtual_dir vd_lookup (zfs_fh *fh);
extern virtual_dir vd_lookup_name (virtual_dir parent, string *name);
extern internal_dentry dentry_lookup (zfs_fh *fh);
extern internal_dentry dentry_lookup_name (volume vol, internal_dentry parent,
					   string *name);
extern int32_t internal_dentry_lock (unsigned int level, volume *volp,
				     internal_dentry *dentryp, zfs_fh *tmp_fh);
extern void internal_dentry_unlock (volume vol, internal_dentry dentry);
extern int32_t internal_dentry_lock2 (unsigned int level1, unsigned int level2,
				      volume *volp, internal_dentry *dentry1p,
				      internal_dentry *dentry2p,
				      zfs_fh *tmp_fh1, zfs_fh *tmp_fh2);
extern bool set_master_fh (volume vol, internal_fh fh, zfs_fh *master_fh);
extern void print_fh_htab (FILE *f);
extern void debug_fh_htab (void);
extern void print_subdentries (FILE *f, internal_dentry dentry);
extern void debug_subdentries (internal_dentry dentry);

extern internal_dentry internal_dentry_link (internal_dentry orig, volume vol,
					     internal_dentry parent,
					     string *name);
extern void internal_dentry_move (internal_dentry *from_dirp, string *from_name,
				  internal_dentry *to_dirp, string *to_name,
				  volume *volp, zfs_fh *from_fh, zfs_fh *to_fh);
extern void internal_dentry_destroy (internal_dentry dentry,
				     bool clear_volume_root);
extern internal_dentry create_conflict (volume vol, internal_dentry dir,
					string *name, zfs_fh *local_fh,
					fattr *attr);
extern internal_dentry add_file_to_conflict_dir (volume vol,
						 internal_dentry conflict,
						 bool exists, zfs_fh *fh,
						 fattr *attr, metadata *meta);
extern bool try_resolve_conflict (volume vol, internal_dentry conflict);
extern internal_dentry conflict_local_dentry (internal_dentry conflict);
extern internal_dentry conflict_remote_dentry (internal_dentry conflict);
extern internal_dentry conflict_other_dentry (internal_dentry conflict,
					      internal_dentry dentry);
extern void cancel_conflict (volume vol, internal_dentry conflict);

extern virtual_dir virtual_dir_create (virtual_dir parent, const char *name);
extern void virtual_dir_destroy (virtual_dir vd);
extern virtual_dir virtual_root_create (void);
extern void virtual_root_destroy (virtual_dir root);
extern virtual_dir virtual_mountpoint_create (volume vol);
extern void virtual_mountpoint_destroy (volume vol);
extern void virtual_dir_set_fattr (virtual_dir vd);
extern void print_virtual_tree (FILE *f);
extern void debug_virtual_tree (void);

extern void initialize_fh_c (void);
extern void cleanup_fh_c (void);

#endif
