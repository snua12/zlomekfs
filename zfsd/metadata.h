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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "zfs_prot.h"
#include "hardlink-list.h"
#include "journal.h"

/* Depth of directory tree for saving metadata about files.  */
extern unsigned int metadata_tree_depth;

/*Type of metadata.  */
typedef enum metadata_type_def
{
  /* Generic metadata hashed by local file handle.  */
  METADATA_TYPE_METADATA,

  /* Mapping of master file handle to local file handle.  */
  METADATA_TYPE_FH_MAPPING,

  /* Intervals updated from master node.  */
  METADATA_TYPE_UPDATED,

  /* Intervals modified by local node.  */
  METADATA_TYPE_MODIFIED,

  /* List of hardlinks for file handle.  */
  METADATA_TYPE_HARDLINKS,

  /* Journal of modifications of a directory.  */
  METADATA_TYPE_JOURNAL
} metadata_type;

/* The size of char array for file name stored in hash table.
   Longer names are stored in separate files together with hardlinks.  */
#define METADATA_NAME_SIZE 36

/* Metadata for file.  */
typedef struct metadata_def
{
  uint32_t slot_status;		/* status of slot (empty, deleted, valid) */
  uint32_t flags;		/* see METADATA_* below */
  uint32_t dev;			/* device of the file */
  uint32_t ino;			/* inode of the file */
  uint32_t gen;			/* generation of the file */
  zfs_fh master_fh;		/* master file handle */
  uint64_t local_version;	/* local version (is it needed?) */
  uint64_t master_version;	/* version on server/version got from server */
  uint32_t parent_dev;		/* device of the parent of the file */
  uint32_t parent_ino;		/* inode of the parent of the file */
  char name[METADATA_NAME_SIZE];/* file name */
} metadata;

/* File handle mapping.  */
typedef struct fh_mapping_def
{
  uint32_t slot_status;		/* status of slot (empty, deleted, valid) */
  zfs_fh master_fh;		/* master file handle */
  zfs_fh local_fh;		/* local file handle */
} fh_mapping;

#define METADATA_COMPLETE	1	/* file is complete */
#define METADATA_MODIFIED	2	/* file is modified */

#include "volume.h"
#include "fh.h"

extern hashval_t metadata_hash (const void *x);
extern int metadata_eq (const void *x, const void *y);

#if BYTE_ORDER != LITTLE_ENDIAN
extern void metadata_decode (void *x);
extern void metadata_encode (void *x);
#else
#define metadata_decode NULL
#define metadata_encode NULL
#endif

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
extern bool get_metadata (volume vol, zfs_fh *fh, metadata *meta);
extern bool find_metadata (volume vol, zfs_fh *fh, metadata *meta);
extern bool flush_metadata (volume vol, internal_fh fh);
extern bool set_metadata (volume vol, internal_fh fh, uint32_t flags,
			  uint64_t local_version, uint64_t master_version);
extern bool set_metadata_flags (volume vol, internal_fh fh, uint32_t flags);
extern bool set_metadata_master_fh (volume vol, internal_fh fh,
				    zfs_fh *master_fh);
extern bool inc_local_version (volume vol, internal_fh fh);
extern bool delete_metadata (volume vol, uint32_t dev, uint32_t ino,
			     uint32_t parent_dev, uint32_t parent_ino,
			     char *name);
extern bool load_interval_trees (volume vol, internal_fh fh);
extern bool save_interval_trees (volume vol, internal_fh fh);
extern bool init_hardlinks (volume vol, zfs_fh *fh, metadata *meta,
			    hardlink_list hl);
extern bool metadata_hardlink_insert (volume vol, zfs_fh *fh,
				      uint32_t parent_dev, uint32_t parent_ino,
				      char *name);
extern bool metadata_hardlink_replace (volume vol, zfs_fh *fh,
				       uint32_t old_parent_dev,
				       uint32_t old_parent_ino, char *old_name,
				       uint32_t new_parent_dev,
				       uint32_t new_parent_ino, char *new_name);
extern char *get_local_path_from_metadata (volume vol, zfs_fh *fh);
extern bool read_journal (volume vol, internal_fh fh);
extern bool write_journal (volume vol, internal_fh fh);
extern bool add_journal_entry (volume vol, internal_fh fh, zfs_fh *local_fh,
			       zfs_fh *master_fh, char *name,
			       journal_operation_t oper);
extern bool add_journal_entry_st (volume vol, internal_fh fh, struct stat *st,
				  char *name, journal_operation_t oper);

extern void initialize_metadata_c (void);
extern void cleanup_metadata_c (void);
#endif
