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
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "server.h"
#include "varray.h"

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

/* Hash function for svc_fh FH.  */
#define SVC_FH_HASH(FH) (crc32_buffer ((FH), sizeof (svc_fh)))

/* Compare two svc_fh FH1 and FH2.  */
#define SVC_FH_EQ(FH1, FH2) ((FH1).ino == (FH2).ino		\
			     && (FH1).dev == (FH2).dev		\
			     && (FH1).vid == (FH2).vid		\
			     && (FH1).sid == (FH2).sid)


/* Hash function for internal_fh FH, computed from client_fh.  */
#define INTERNAL_FH_HASH(FH)						\
  (crc32_buffer (&(FH)->client_fh, sizeof (svc_fh)))

/* Hash function for internal_fh FH, computed from parent_fh and name.  */
#define INTERNAL_FH_HASH_NAME(FH)					\
  (crc32_update (crc32_string ((FH)->name),				\
		 &(FH)->parent->client_fh, sizeof (svc_fh)))

/* Destroy the virtual mountpoint of volume VOL.  */
#define virtual_mountpoint_destroy(VOL) (virtual_dir_destroy ((VOL)->root_vd))

/* Structure of the file handle being sent between daemon and kernel
   and between daemons.  */
typedef struct svc_fh_def
{
  /* Server ID.  */
  unsigned int sid;

  /* Volume ID.  */
  unsigned int vid;

  /* Device ... */
  unsigned int dev;

  /* ... and inode number of the file.  */
  unsigned int ino;
} svc_fh;

/* Forward definitions.  */
typedef struct internal_fh_def *internal_fh;
typedef struct virtual_dir_def *virtual_dir;

#include "volume.h"

/* Internal information about file handle.  */
struct internal_fh_def
{
  /* File handle for client, key for hash table.  */
  svc_fh client_fh;

  /* File handle for server.  */
  svc_fh server_fh;

  /* Pointer to file handle of the parent directory.  */
  internal_fh parent;

#if 0
  /* Pointer to the virtual file handle which contains this internal_fh.  */
  struct virtual_dir_def *vd;
  volume vol;
#endif
  
  /* File name.  */
  char *name;

  /* Open file descriptor.  */
  int fd;
};

/* Structure of a virtual directory (element of mount tree).  */
struct virtual_dir_def
{
  /* Handle of this node.  */
  svc_fh fh;

  /* Pointer to parent virtual directory.  */
  virtual_dir parent;

  /* Directory name.  */
  char *name;

  /* Subdirectories (of type 'struct internal_fh_def *').  */
  varray subdirs;

  /* Index in parent's list of subdirectories.  */
  unsigned int subdir_index;

  /* Number of active leaves
     (i.e. the number of mountpoints of volumes which are VOLUME_ACTIVE_P).  */
  unsigned int active;

  /* Total number of leaves.  */
  unsigned int total;
  
  /* Volume which is mounted here.  */
  volume vol;
};

/* File handle of ZFS root.  */
extern svc_fh root_fh;

/* Allocation pool for file handles.  */
extern alloc_pool fh_pool;

extern hash_t internal_fh_hash (const void *x);
extern hash_t internal_fh_hash_name (const void *x);
extern int internal_fh_eq (const void *xx, const void *yy);
extern int internal_fh_eq_name (const void *xx, const void *yy);
extern void internal_fh_del (void *x);
extern int fh_lookup (svc_fh *fh, volume *volp, internal_fh *ifhp,
		      virtual_dir *vdp);
extern virtual_dir vd_lookup_name (virtual_dir parent, const char *name);
extern internal_fh fh_lookup_name (volume vol, internal_fh parent,
				   const char *name);
extern internal_fh internal_fh_create (svc_fh *client_fh, svc_fh *server_fh,
				       internal_fh parent, volume vol,
				       const char *name);
extern void internal_fh_destroy (internal_fh fh, volume vol);
extern void print_fh_htab (FILE *f, htab_t htab);
extern void debug_fh_htab (htab_t htab);

extern virtual_dir virtual_dir_create (virtual_dir parent, const char *name);
extern void virtual_dir_destroy (virtual_dir vd);
extern virtual_dir virtual_root_create ();
extern void virtual_root_destroy (virtual_dir root);
extern virtual_dir virtual_mountpoint_create (volume vol);
extern void print_virtual_tree (FILE *f);
extern void debug_virtual_tree ();

extern void initialize_fh_c ();
extern void cleanup_fh_c ();

#endif
