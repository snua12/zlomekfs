/*! \file
    \brief Functions for file versioning.  */

/* Copyright (C) 2010 Rastislav Wartiak

   This file is part of zlomekFS.

   zlomekFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   zlomekFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef VERSION_H
#define VERSION_H

#ifdef VERSIONS

#include "system.h"
#include "zfs-prot.h"
#include "fh.h"
#include "file.h"

/*! \brief Maximum block size for versioning
 *
 * \see ZFS_MAXDATA
 *
 */
#define ZFS_VERSION_BLOCK_SIZE ZFS_MAXDATA

#define VERSION_MAX_SPECIFIER_LENGTH 21

#define VERSION_NAME_SPECIFIER_C '@'
#define VERSION_NAME_SPECIFIER_S "@"
#define VERSION_TIMESTAMP "%Y-%m-%d-%H-%M-%S"

/*! Mark file as truncated.  */
#define MARK_FILE_TRUNCATED(FH)           \
  ({ \
    (FH)->file_truncated = true; \
  })

/*! Unmark file as truncated.  */
#define UNMARK_FILE_TRUNCATED(FH)           \
  ({ \
    (FH)->file_truncated = false; \
  })

/*! True when the NAME is a version file.  */
#define VERSION_FILENAME_P(NAME)          \
  (strchr ((NAME), VERSION_NAME_SPECIFIER_C) != NULL)

/*! Was file as truncated before opening?  */
#define WAS_FILE_TRUNCATED(FH) ((FH)->file_truncated)

typedef struct version_item_def
{
  time_t stamp;
  char *name;
  char *path;
  interval_tree intervals;
} version_item;

struct dirhtab_item_def
{
  char *name;
  time_t stamp;
  long ino;
};

#define CLEAR_VERSION_ITEM(I) \
{\
  I.stamp = 0; \
  if (I.name) free (I.name); \
  I.name = NULL; \
  if (I.path) free (I.path); \
  I.path = NULL; \
  if (I.intervals) interval_tree_destroy (I.intervals); \
  I.intervals = NULL; \
}

extern void version_create_dirhtab (internal_dentry dentry);
extern int32_t version_readdir_from_dirhtab (dir_list *list, internal_dentry dentry, int32_t cookie,
                                             readdir_data *data, filldir_f filldir);
extern int32_t version_readdir_fill_dirhtab (internal_dentry dentry, time_t stamp, long ino, char *name);
extern bool version_load_interval_tree (internal_fh fh);
extern bool version_save_interval_trees (internal_fh fh);
extern int32_t version_generate_filename (char *path, string *verpath);
extern int32_t version_create_file_with_attr (char *path, internal_dentry dentry, bool with_size);
extern int32_t version_create_file (internal_dentry dentry, volume vol);
extern int32_t version_close_file (internal_fh fh, bool tidy);
extern int32_t version_truncate_file (internal_dentry dentry, char *path);
extern int32_t version_unlink_file (char *path);
extern int32_t version_find_version (char *dir, string *name, time_t stamp);
extern int32_t version_get_filename_stamp(char *name, time_t *stamp, int *orgnamelen);
extern int32_t version_is_directory (string *dst, char *dir, string *name, time_t stamp, time_t *dirstamp, int orgnamelen);
extern int32_t version_build_intervals (internal_dentry dentry, volume vol);
extern int32_t version_read_old_data (internal_dentry dentry, uint64_t start, uint64_t end, char *buf);
extern int32_t version_rename_source(char *path);
extern int32_t version_copy_data (int fd, int fdv, uint64_t offset, uint32_t length, data_buffer *newdata);
#endif /* VERSIONS */

#endif /* VERSION_H */
