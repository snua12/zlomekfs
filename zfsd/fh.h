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
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "varray.h"
#include "fibheap.h"
#include "interval.h"
#include "string-list.h"
#include "zfs_prot.h"
#include "util.h"

#define VIRTUAL_DEVICE 0	/* Device number of device with virtual
				   directories */
#define ROOT_INODE 0		/* Inode number of the root dir of ZFS */

/* Is the FH virtual?  */
#define VIRTUAL_FH_P(FH) ((FH).vid == VIRTUAL_DEVICE)

/* Is FH the virtual root?  */
#define VIRTUAL_ROOT_P(FH) ((FH).ino == ROOT_INODE		\
			    && (FH).dev == VIRTUAL_DEVICE	\
			    && (FH).vid == VOLUME_ID_NONE	\
			    && (FH).sid == NODE_ANY)

/* Mark the ZFS file handle FH to be undefined.  */
#define zfs_fh_undefine(FH) (sizeof (FH) == sizeof (zfs_fh)		\
			     ? memset (&(FH), -1, sizeof (zfs_fh))	\
			     : (abort (), (void *) 0))

/* Return true if the ZFS file handle FH is undefined.  */
#define zfs_fh_undefined(FH) (bytecmp (&(FH), -1, sizeof (zfs_fh)))

/* Hash function for zfs_fh FH.  */
#define ZFS_FH_HASH(FH) (crc32_buffer ((FH), sizeof (zfs_fh)))

/* Return true if FH1 and FH2 are the same.  */
#define ZFS_FH_EQ(FH1, FH2) ((FH1).ino == (FH2).ino		\
			     && (FH1).dev == (FH2).dev		\
			     && (FH1).vid == (FH2).vid		\
			     && (FH1).sid == (FH2).sid)

/* Hash function for internal dentry D, computed from fh->local_fh.  */
#define INTERNAL_DENTRY_HASH(D)						\
  (ZFS_FH_HASH (&(D)->fh->local_fh))

/* Hash function for internal dentry D, computed from parent->fh and name.  */
#define INTERNAL_DENTRY_HASH_NAME(D)					\
  (crc32_update (crc32_string ((D)->name),				\
		 &(D)->parent->fh->local_fh, sizeof (zfs_fh)))

/* Destroy dentry NAME in directory DIR (whose file handle is DIR_FH)
   on volume VOL.  Delete PATH from hardlinks.  */
#define DESTROY_DENTRY(VOL, DIR, NAME, DIR_FH, PATH)			\
  do {									\
    internal_dentry dentry;						\
									\
    if (ENABLE_CHECKING_VALUE && (DIR)->fh->level == LEVEL_UNLOCKED)	\
      abort();								\
									\
    dentry = dentry_lookup_name ((DIR), (NAME));			\
    if (dentry)								\
      {									\
	if ((PATH) && dentry->fh->hardlinks)				\
	  string_list_delete (dentry->fh->hardlinks, (PATH));		\
	release_dentry ((DIR));						\
	zfsd_mutex_unlock (&(VOL)->mutex);				\
									\
	internal_dentry_destroy (dentry, true);				\
									\
	zfsd_mutex_unlock (&fh_mutex);					\
									\
	r2 = zfs_fh_lookup_nolock (&(DIR_FH), &(VOL), &(DIR), NULL);	\
	if (ENABLE_CHECKING_VALUE && r2 != ZFS_OK)			\
	  abort ();							\
      }									\
  } while (0)

/* "Lock" level of the file handle or virtual directory.  */
#define LEVEL_UNLOCKED	0
#define LEVEL_SHARED	1
#define LEVEL_EXCLUSIVE	2

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

  /* File handle for server.  */
  zfs_fh master_fh;

  /* File attributes.  */
  fattr attr;

  /* Metadata.  */
  metadata meta;

  /* Contained directory entries (of type 'struct internal_dentry_def *').  */
  varray subdentries;

  /* Chain of capabilities associated with this file handle.  */
  internal_cap cap;

  /* Number of directory entries associated with this file handle.  */
  unsigned int ndentries;

  /* Updated intervals.  */
  interval_tree updated;

  /* Modified intervals.  */
  interval_tree modified;

  /* Number of users of interval trees.  */
  unsigned int interval_tree_users;

  /* List of hardlinks.  */
  string_list hardlinks;

  /* "Lock" level of the file handle.  */
  unsigned int level;

  /* Number of current users of the file handle.  */
  unsigned int users;

  /* Owner of the file handle if level == LEVEL_EXCLUSIVE.  */
  pthread_t owner;

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

/* Internal directory entry.  */
struct internal_dentry_def
{
  /* Mutex is not needed here because we can use FH->MUTEX
     because FH is constant for each internal dentry.  */

  /* Pointer to internal dentry of the parent directory.  */
  internal_dentry parent;

  /* File name.  */
  char *name;

  /* Internal file handle associated with this dentry.  */
  internal_fh fh;

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
  char *name;

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

/* Mutes for file handles and dentries.  */
extern pthread_mutex_t fh_mutex;

/* Hash table of virtual directories, searched by fh.  */
extern htab_t vd_htab;

/* Mutex for virtual directories.  */
extern pthread_mutex_t vd_mutex;

/* Thread ID of thread freeing dentries unused for a long time.  */
extern pthread_t cleanup_dentry_thread;

/* This mutex is locked when cleanup dentry thread is in sleep.  */
extern pthread_mutex_t cleanup_dentry_thread_in_syscall;

extern int32_t zfs_fh_lookup (zfs_fh *fh, volume *volp,
			      internal_dentry *dentryp, virtual_dir *vdp);
extern int32_t zfs_fh_lookup_nolock (zfs_fh *fh, volume *volp,
				     internal_dentry *dentryp,
				     virtual_dir *vdp);
extern void release_dentry (internal_dentry dentry);
extern internal_dentry get_dentry (zfs_fh *local_fh, zfs_fh *master_fh,
				   volume vol, internal_dentry dir,
				   char *name, fattr *attr);
extern virtual_dir vd_lookup (zfs_fh *fh);
extern virtual_dir vd_lookup_name (virtual_dir parent, const char *name);
extern internal_dentry dentry_lookup (zfs_fh *fh);
extern internal_dentry dentry_lookup_name (internal_dentry parent,
					   const char *name);
extern int32_t internal_dentry_lock (unsigned int level, volume *volp,
				     internal_dentry *dentryp, zfs_fh *tmp_fh);
extern void internal_dentry_unlock (volume vol, internal_dentry dentry);
extern int32_t internal_dentry_lock2 (unsigned int level1, unsigned int level2,
				      volume *volp, internal_dentry *dentry1p,
				      internal_dentry *dentry2p,
				      zfs_fh *tmp_fh1, zfs_fh *tmp_fh2);
extern internal_dentry internal_dentry_create (zfs_fh *local_fh,
					       zfs_fh *master_fh, volume vol,
					       internal_dentry parent,
					       char *name,
					       fattr *attr);
extern internal_dentry internal_dentry_link (internal_fh fh, volume vol,
					     internal_dentry parent,
					     char *name);
extern bool internal_dentry_move (internal_dentry dentry, volume vol,
				  internal_dentry dir, char *name);
extern void internal_dentry_destroy (internal_dentry dentry,
				     bool clear_volume_root);
extern void print_fh_htab (FILE *f, htab_t htab);
extern void debug_fh_htab (htab_t htab);

extern virtual_dir virtual_dir_create (virtual_dir parent, const char *name);
extern void virtual_dir_destroy (virtual_dir vd);
extern virtual_dir virtual_root_create ();
extern void virtual_root_destroy (virtual_dir root);
extern virtual_dir virtual_mountpoint_create (volume vol);
extern void virtual_mountpoint_destroy (volume vol);
extern void virtual_dir_set_fattr (virtual_dir vd);
extern void print_virtual_tree (FILE *f);
extern void debug_virtual_tree ();

extern void initialize_fh_c ();
extern void cleanup_fh_c ();

#endif
