/* File handle functions.
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

#ifndef FH_H
#define FH_H

/* Forward declaration.  */
typedef struct volume_def *volume;

#include "system.h"
#include <stdio.h>
#include "pthread.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "varray.h"
#include "zfs_prot.h"

#define VIRTUAL_DEVICE 0	/* Device number of device with virtual
				   directories */
#define ROOT_INODE 0		/* Inode number of the root dir of ZFS */

/* Is the FH virtual?  */
#define VIRTUAL_FH_P(FH) ((FH).vid == VIRTUAL_DEVICE)

/* Is FH the virtual root?  */
#define VIRTUAL_ROOT_P(FH) ((FH).ino == ROOT_INODE		\
			    && (FH).dev == VIRTUAL_DEVICE	\
			    && (FH).vid == VOLUME_ID_NONE	\
			    && (FH).sid == SERVER_ANY)

/* Hash function for zfs_fh FH.  */
#define ZFS_FH_HASH(FH) (crc32_buffer ((FH), sizeof (zfs_fh)))

/* Compare two zfs_fh FH1 and FH2.  */
#define ZFS_FH_EQ(FH1, FH2) ((FH1).ino == (FH2).ino		\
			     && (FH1).dev == (FH2).dev		\
			     && (FH1).vid == (FH2).vid		\
			     && (FH1).sid == (FH2).sid)


/* Hash function for internal_fh FH, computed from local_fh.  */
#define INTERNAL_FH_HASH(FH)						\
  (crc32_buffer (&(FH)->local_fh, sizeof (zfs_fh)))

/* Hash function for internal_fh FH, computed from parent_fh and name.  */
#define INTERNAL_FH_HASH_NAME(FH)					\
  (crc32_update (crc32_string ((FH)->name),				\
		 &(FH)->parent->local_fh, sizeof (zfs_fh)))

/* Destroy the virtual mountpoint of volume VOL.  */
#define virtual_mountpoint_destroy(VOL) (virtual_dir_destroy ((VOL)->root_vd))

/* Forward definitions.  */
typedef struct internal_fh_def *internal_fh;
typedef struct virtual_dir_def *virtual_dir;

#include "volume.h"

/* Internal information about file handle.  */
struct internal_fh_def
{
  /* File handle for client, key for hash table.  */
  zfs_fh local_fh;

  /* File handle for server.  */
  zfs_fh master_fh;

  /* Pointer to file handle of the parent directory.  */
  internal_fh parent;

  /* File name.  */
  char *name;

  /* File attributes.  */
  fattr attr;

  /* Directory entries (of type 'struct internal_fh_def *').  */
  varray dentries;

  /* Index in parent's list of directory entries.  */
  unsigned int dentry_index;
};

/* Structure of a virtual directory (element of mount tree).  */
struct virtual_dir_def
{
  /* Handle of this node.  */
  zfs_fh fh;

  /* Pointer to parent virtual directory.  */
  virtual_dir parent;

  /* Directory name.  */
  char *name;
  
  /* Volume which is mounted here.  */
  volume vol;

  /* Directory attributes.  */
  fattr attr;

  /* Subdirectories (of type 'struct virtual_dir_def *').  */
  varray subdirs;

  /* Index in parent's list of subdirectories.  */
  unsigned int subdir_index;

  /* Total number of mountpoints in subtree (including current node).  */
  unsigned int n_mountpoints;
};

/* File handle of ZFS root.  */
extern zfs_fh root_fh;

/* Mutex for fh_pool.  */
extern pthread_mutex_t fh_pool_mutex;

extern hash_t internal_fh_hash (const void *x);
extern hash_t internal_fh_hash_name (const void *x);
extern int internal_fh_eq (const void *xx, const void *yy);
extern int internal_fh_eq_name (const void *xx, const void *yy);
extern void internal_fh_del (void *x);
extern bool fh_lookup (zfs_fh *fh, volume *volp, internal_fh *ifhp,
		       virtual_dir *vdp);
extern virtual_dir vd_lookup_name (virtual_dir parent, const char *name);
extern internal_fh fh_lookup_name (volume vol, internal_fh parent,
				   const char *name);
extern internal_fh internal_fh_create (zfs_fh *local_fh, zfs_fh *master_fh,
				       internal_fh parent, volume vol,
				       const char *name, fattr *attr);
extern void internal_fh_destroy (internal_fh fh, volume vol);
extern void print_fh_htab (FILE *f, htab_t htab);
extern void debug_fh_htab (htab_t htab);

extern virtual_dir virtual_dir_create (virtual_dir parent, const char *name);
extern void virtual_dir_destroy (virtual_dir vd);
extern virtual_dir virtual_root_create ();
extern void virtual_root_destroy (virtual_dir root);
extern virtual_dir virtual_mountpoint_create (volume vol);
extern void virtual_dir_set_fattr (virtual_dir vd);
extern void print_virtual_tree (FILE *f);
extern void debug_virtual_tree ();

extern void initialize_fh_c ();
extern void cleanup_fh_c ();

#endif
