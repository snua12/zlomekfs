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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "pthread.h"
#include "data-coding.h"
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"

/* Data for file descriptor.  */
typedef struct internal_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor */
  time_t last_use;		/* time of last use of the file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
  int busy;			/* number of threads using file descriptor */
} internal_fd_data_t;

extern int zfs_create (create_res *res, zfs_fh *dir, string *name,
		       unsigned int flags, sattr *attr);
extern int zfs_open (zfs_cap *cap, zfs_fh *fh, unsigned int flags);
extern int zfs_close (zfs_cap *cap);
extern int zfs_readdir (DC *dc, zfs_cap *cap, int cookie, unsigned int count);
extern int zfs_read (DC *dc, zfs_cap *cap, uint64_t offset, unsigned int count);
extern int zfs_write (write_res *res, write_args *args);
extern void initialize_file_c ();
extern void cleanup_file_c ();

#endif
