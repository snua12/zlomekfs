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
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"

extern void fattr_from_struct_stat (fattr *attr, struct stat *st, volume vol);
extern int local_getattr (fattr *attr, char *path, volume vol);
extern int zfs_getattr (fattr *fa, zfs_fh *fh);
extern int zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa);
extern int zfs_unlink (zfs_fh *dir, string *name);

#endif
