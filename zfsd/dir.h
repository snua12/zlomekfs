/* Directory operations.
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

#ifndef DIR_H
#define DIR_H

#include "system.h"
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"
#include "zfs_prot.h"

extern char *build_local_path (volume vol, internal_fh fh);
extern char *build_local_path_name (volume vol, internal_fh fh,
				    const char *name);
extern int validate_operation_on_virtual_directory (virtual_dir pvd,
						    string *name,
						    internal_fh *idir);
extern int update_volume_root (volume vol, internal_fh *ifh);
extern void fattr_from_struct_stat (fattr *attr, struct stat *st, volume vol);
extern int local_getattr (fattr *attr, char *path, volume vol);
extern int zfs_getattr (fattr *fa, zfs_fh *fh);
extern int zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa);
extern int zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path);
extern int zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name);
extern int zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name, sattr *attr);
extern int zfs_rmdir (zfs_fh *dir, string *name);
extern int zfs_unlink (zfs_fh *dir, string *name);
extern int zfs_mknod (zfs_fh *dir, string *name, sattr *attr, ftype type,
		      unsigned int rdev);

#endif
