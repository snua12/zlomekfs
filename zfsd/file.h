/* File operations.
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

#ifndef FILE_H
#define FILE_H

#include "system.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "pthread.h"
#include "data-coding.h"
#include "hashtab.h"
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"
#include "fibheap.h"

/* Data for file descriptor.  */
typedef struct internal_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
  fibnode heap_node;		/* node of heap whose data is this structure  */
} internal_fd_data_t;

/* Data for supplementary functions of readdir.  */
typedef struct readdir_data_def
{
  uint32_t written;
  uint32_t count;
} readdir_data;

/* Structure holding entries for filldir_htab.  */
typedef struct filldir_htab_entries_def
{
  htab_t htab;
  int32_t last_cookie;
} filldir_htab_entries;

/* Function called to add one directory entry to list.  */
typedef bool (*filldir_f) (uint32_t ino, int32_t cookie, char *name,
			   uint32_t name_len, dir_list *list,
			   readdir_data *data);

#include "cap.h"
extern int32_t local_close (internal_cap cap);
extern int32_t remote_close (internal_cap cap, internal_dentry dentry,
			     volume vol);
extern int32_t local_create (create_res *res, int *fdp, internal_dentry dir,
			     string *name, uint32_t flags, sattr *attr,
			     volume vol);
extern int32_t remote_create (create_res *res, internal_dentry dir,
			      string *name, uint32_t flags, sattr *attr,
			      volume vol);
extern int32_t zfs_create (create_res *res, zfs_fh *dir, string *name,
			   uint32_t flags, sattr *attr);
extern int32_t local_open (internal_cap icap, uint32_t flags,
			   internal_dentry dentry, volume vol);
extern int32_t remote_open (zfs_cap *cap, internal_cap icap, uint32_t flags,
			    internal_dentry dentry, volume vol);
extern int32_t zfs_open (zfs_cap *cap, zfs_fh *fh, uint32_t flags);
extern int32_t zfs_close (zfs_cap *cap);
extern bool filldir_encode (uint32_t ino, int32_t cookie, char *name,
			    uint32_t name_len, dir_list *list,
			    readdir_data *data);
extern bool filldir_array (uint32_t ino, int32_t cookie, char *name,
			   uint32_t name_len, dir_list *list,
			   ATTRIBUTE_UNUSED readdir_data *data);
extern hash_t filldir_htab_hash (const void *x);
extern int filldir_htab_eq (const void *xx, const void *yy);
extern void filldir_htab_del (void *xx);
extern bool filldir_htab (uint32_t ino, int32_t cookie, char *name,
			  uint32_t name_len, dir_list *list,
			  ATTRIBUTE_UNUSED readdir_data *data);
extern int32_t local_readdir (dir_list *list, internal_cap cap,
			      internal_dentry dentry, virtual_dir vd,
			      int32_t cookie, readdir_data *data, volume vol,
			      filldir_f filldir);
extern int32_t remote_readdir (dir_list *list, internal_cap cap,
			       internal_dentry dentry, int32_t cookie,
			       readdir_data *data, volume vol,
			       filldir_f filldir);
extern int32_t zfs_readdir (dir_list *list, zfs_cap *cap, int32_t cookie,
			    uint32_t count, filldir_f filldir);
extern int32_t zfs_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
			 uint64_t offset, uint32_t count, bool update);
extern int32_t zfs_write (write_res *res, write_args *args);

extern int32_t full_local_readdir (zfs_fh *fh, filldir_htab_entries *entries);
extern int32_t full_remote_readdir (zfs_fh *fh, filldir_htab_entries *entries);
extern int32_t full_local_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
				uint64_t offset, uint32_t count);
extern int32_t full_remote_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
				 uint64_t offset, uint32_t count);
extern int32_t full_local_write (uint32_t *rcount, void *buffer, zfs_cap *cap,
				 uint64_t offset, uint32_t count);
extern int32_t local_md5sum (md5sum_res *res, md5sum_args *args);
extern int32_t remote_md5sum (md5sum_res *res, md5sum_args *args);
extern void initialize_file_c ();
extern void cleanup_file_c ();

#endif
