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

#if (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX) > 0xffffff
#error access mode flags are too big
#endif

/* Get mode from MODE from struct stat.  */
#define GET_MODE(MODE) ((MODE) & (S_IRWXU | S_IRWXG | S_IRWXO		\
				  | S_ISUID | S_ISGID | S_ISVTX))

/* Compute a combination of MODE and TYPE.  */
#define GET_MODETYPE(MODE, TYPE) (GET_MODE (MODE) | ((TYPE) << 24))

/* Get mode from MODETYPE.  */
#define GET_MODETYPE_MODE(MODETYPE) ((MODETYPE) & UINT32_C (0xffffff))

/* Get type from MODETYPE.  */
#define GET_MODETYPE_TYPE(MODETYPE) ((MODETYPE) >> 24)

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
  METADATA_TYPE_JOURNAL,

  /* File which is not accessible in the directory tree yet.  */
  METADATA_TYPE_SHADOW
} metadata_type;

/* The size of char array for file name stored in hash table.
   Longer names are stored in separate files together with hardlinks.  */
#define METADATA_NAME_SIZE 52

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
  uint32_t modetype;		/* original access rights and file type */
  uint32_t uid;			/* original owner */
  uint32_t gid;			/* original group */
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
#define METADATA_UPDATED_TREE	2	/* file has an updated interval tree */
#define METADATA_MODIFIED_TREE	4	/* file has a modified interval tree */
#define METADATA_SHADOW		16	/* file is in shadow */
#define METADATA_SHADOW_TREE	32	/* dir is a part of shadow tree */

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

extern ftype zfs_mode_to_ftype (uint32_t mode);
extern bool init_volume_metadata (volume vol);
extern void close_volume_metadata (volume vol);
extern void close_interval_file (interval_tree tree);
extern void close_journal_file (journal_t journal);
extern bool flush_interval_tree (volume vol, internal_fh fh,
				metadata_type type);
extern bool append_interval (volume vol, internal_fh fh,
			     metadata_type type,
			     uint64_t start, uint64_t end);
extern void set_attr_version (fattr *attr, metadata *meta);
extern bool lookup_metadata (volume vol, zfs_fh *fh, metadata *meta,
			     bool insert);
extern bool get_metadata (volume vol, zfs_fh *fh, metadata *meta);
extern bool get_fh_mapping_for_master_fh (volume vol, zfs_fh *master_fh,
					  fh_mapping *map);
extern bool flush_metadata (volume vol, metadata *meta);
extern bool set_metadata (volume vol, internal_fh fh, uint32_t flags,
			  uint64_t local_version, uint64_t master_version);
extern bool set_metadata_flags (volume vol, internal_fh fh, uint32_t flags);
extern bool set_metadata_master_fh (volume vol, internal_fh fh,
				    zfs_fh *master_fh);
extern bool inc_local_version (volume vol, internal_fh fh);
extern bool inc_local_version_and_modified (volume vol, internal_fh fh);
extern bool delete_metadata (volume vol, metadata *meta, uint32_t dev,
			     uint32_t ino, uint32_t parent_dev,
			     uint32_t parent_ino, string *name);
extern bool delete_metadata_of_created_file (volume vol, zfs_fh *fh,
					     metadata *meta);
extern bool load_interval_trees (volume vol, internal_fh fh);
extern bool save_interval_trees (volume vol, internal_fh fh);
extern bool init_hardlinks (volume vol, zfs_fh *fh, metadata *meta,
			    hardlink_list hl);
extern bool read_hardlinks (volume vol, zfs_fh *fh, metadata *meta,
			    hardlink_list hl);
extern bool metadata_hardlink_insert (volume vol, zfs_fh *fh, metadata *meta,
				      uint32_t parent_dev, uint32_t parent_ino,
				      string *name);
extern bool metadata_hardlink_replace (volume vol, zfs_fh *fh, metadata *meta,
				       uint32_t old_parent_dev,
				       uint32_t old_parent_ino,
				       string *old_name,
				       uint32_t new_parent_dev,
				       uint32_t new_parent_ino,
				       string *new_name, bool shadow);
extern bool metadata_hardlink_set (volume vol, zfs_fh *fh, metadata *meta,
				   uint32_t parent_dev, uint32_t parent_ino,
				   string *name);
extern unsigned int metadata_n_hardlinks (volume vol, zfs_fh *fh,
					  metadata *meta);
extern void get_local_path_from_metadata (string *path, volume vol, zfs_fh *fh);
extern bool read_journal (volume vol, zfs_fh *fh, journal_t journal);
extern bool write_journal (volume vol, zfs_fh *fh, journal_t journal);
extern bool add_journal_entry (volume vol, journal_t journal, zfs_fh *fh,
			       zfs_fh *local_fh, zfs_fh *master_fh,
			       uint64_t master_version, string *name,
			       journal_operation_t oper);
extern bool add_journal_entry_meta (volume vol, journal_t journal, zfs_fh *fh,
				    metadata *meta, string *name,
				    journal_operation_t oper);
extern bool create_shadow_path (string *path, volume vol, zfs_fh *fh,
				string *name);

extern void initialize_metadata_c (void);
extern void cleanup_metadata_c (void);
#endif
