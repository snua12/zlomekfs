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

#include "cap.h"
extern int32_t local_close (internal_cap cap);
extern int32_t zfs_create (create_res *res, zfs_fh *dir, string *name,
			   uint32_t flags, sattr *attr);
extern int32_t zfs_open (zfs_cap *cap, zfs_fh *fh, uint32_t flags);
extern int32_t zfs_close (zfs_cap *cap);
extern int32_t zfs_readdir (DC *dc, zfs_cap *cap, int32_t cookie,
			    uint32_t count);
extern int32_t zfs_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
			 uint64_t offset, uint32_t count, bool update);
extern int32_t zfs_write (write_res *res, write_args *args);

extern int32_t full_local_read (int32_t *rcount, void *buffer, zfs_cap *cap,
				uint64_t offset, uint32_t count);
extern int32_t full_remote_read (int32_t *rcount, void *buffer, zfs_cap *cap,
				 uint64_t offset, uint32_t count);
extern int32_t full_local_write (int32_t *rcount, void *buffer, zfs_cap *cap,
				 uint64_t offset, uint32_t count);
extern int32_t local_md5sum (md5sum_res *res, md5sum_args *args);
extern int32_t remote_md5sum (md5sum_res *res, md5sum_args *args);
extern void initialize_file_c ();
extern void cleanup_file_c ();

#endif
