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

#include "system.h"
#include "alloc-pool.h"
#include "hashtab.h"
#include "server.h"
#include "varray.h"
#include "volume.h"

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
struct internal_fh_def;
struct virtual_dir_def;

/* Internal information about file handle.  */
struct internal_fh_def
{
  /* File handle for client, key for hash table.  */
  svc_fh client_fh;

  /* File handle for server.  */
  svc_fh server_fh;

  /* Pointer to file handle of the parent directory.  */
  struct internal_fh_def *parent;

  /* Pointer to the virtual file handle which contains this internal_fh.  */
  struct virtual_dir_def *vd;
  
  /* File name.  */
  char *name;

  /* Open file descriptor.  */
  int fd;
};

/* Structure of a virtual directory (element of mount tree).  */
struct virtual_dir_def
{
  /* Handle of this node.  */
  struct internal_fh_def *virtual_fh;
  struct internal_fh_def *real_fh;

  /* Pointer to parent virtual directory.  */
  struct virtual_dir_def *parent;

  /* Subdirectories (of type 'struct internal_fh_def *').  */
  varray subdirs;

  /* Index in parent's list of subdirectories.  */
  unsigned int subdir_index;

  /* Number of active leaves
     (i.e. the number of volume mountpoints on connected servers.  */
  unsigned int active;

  /* Total number of leaves.  */
  unsigned int total;
  
  /* Volume which is mounted here.  */
  volume vol;
};

typedef struct internal_fh_def *internal_fh;
typedef struct virtual_dir_def *virtual_dir;

extern internal_fh fh_lookup (svc_fh *fh);
extern internal_fh internal_fh_create (svc_fh *client_fh, svc_fh *server_fh,
				       internal_fh parent, const char *name);
extern void internal_fh_destroy (internal_fh fh);
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
