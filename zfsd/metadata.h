/* Metadata management functions.
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

#ifndef METADATA_H
#define METADATA_H

#include "system.h"
#include <inttypes.h>
#include "zfs_prot.h"

/*Type of metadata.  */
typedef enum metadata_type_def
{
  /* Generic metadata hashed by local file handle.  */
  METADATA_TYPE_LIST,

  /* Intervals updated from master node.  */
  METADATA_TYPE_UPDATED,

  /* Intervals modified by local node.  */
  METADATA_TYPE_MODIFIED,

  /* List of hardlinks for file handle.  */
  METADATA_TYPE_HARDLINKS
} metadata_type;

/* Metadata for file.  */
typedef struct metadata_def
{
  uint32_t slot_status;		/* status of slot (empty, deleted, valid) */
  uint32_t flags;		/* see METADATA_* below */
  uint32_t dev;			/* device of the file */
  uint32_t ino;			/* inode of the file */
  uint64_t local_version;	/* local version (is it needed?) */
  uint64_t master_version;	/* version on server/version got from server */
} metadata;

#define METADATA_COMPLETE	1	/* file is complete */
#define METADATA_MODIFIED	2	/* file is modified */

#include "volume.h"
#include "fh.h"

extern bool init_volume_metadata (volume vol);
extern void close_volume_metadata (volume vol);
extern void close_interval_file (interval_tree tree);
extern bool init_interval_tree (volume vol, internal_fh fh,
				metadata_type type);
extern bool flush_interval_tree (volume vol, internal_fh fh,
				metadata_type type);
extern bool free_interval_tree (volume vol, internal_fh fh,
				metadata_type type);
extern bool append_interval (volume vol, internal_fh fh,
			     metadata_type type,
			     uint64_t start, uint64_t end);
extern void set_attr_version (fattr *attr, metadata *meta);
extern bool init_metadata (volume vol, internal_fh fh);
extern bool set_metadata (volume vol, internal_fh fh, uint32_t flags,
			  uint64_t local_version, uint64_t master_version);
extern bool set_metadata_flags (volume vol, internal_fh fh, uint32_t flags);
extern bool inc_local_version (volume vol, internal_fh fh);
extern bool delete_metadata (volume vol, uint32_t dev, uint32_t ino,
			     char *path);
extern bool load_interval_trees (volume vol, internal_fh fh);
extern bool save_interval_trees (volume vol, internal_fh fh);
extern bool init_hardlinks (volume vol, internal_fh fh);
extern bool flush_hardlinks (volume vol, internal_fh fh);

extern void initialize_metadata_c ();
extern void cleanup_metadata_c ();
#endif
