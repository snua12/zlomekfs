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
#include <inttypes.h>
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"
#include "zfs_prot.h"

extern char *build_local_path (volume vol, internal_dentry dentry);
extern char *build_local_path_name (volume vol, internal_dentry dentry,
				    const char *name);
extern int32_t validate_operation_on_virtual_directory (virtual_dir pvd,
							string *name,
							internal_dentry *dir);
extern int32_t update_volume_root (volume vol, internal_dentry *dentry);
extern void fattr_from_struct_stat (fattr *attr, struct stat *st, volume vol);
extern int32_t local_getattr_path (fattr *attr, char *path, volume vol);
extern int32_t local_getattr (fattr *attr, internal_dentry dentry, volume vol);
extern int32_t remote_getattr (fattr *attr, internal_dentry dentry, volume vol);
extern int32_t zfs_getattr (fattr *fa, zfs_fh *fh);
extern int32_t local_setattr_path (fattr *fa, char *path, sattr *sa,
				   volume vol);
extern int32_t local_setattr (fattr *fa, internal_dentry dentry, sattr *sa,
			      volume vol);
extern int32_t zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa);
extern int32_t zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path);
extern int32_t zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name);
extern int32_t zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name,
			  sattr *attr);
extern int32_t zfs_rmdir (zfs_fh *dir, string *name);
extern int32_t zfs_rename (zfs_fh *from_dir, string *from_name,
			   zfs_fh *to_dir, string *to_name);
extern int32_t zfs_link (zfs_fh *from, zfs_fh *dir, string *name);
extern int32_t zfs_unlink (zfs_fh *dir, string *name);
extern int32_t zfs_readlink (read_link_res *res, zfs_fh *fh);
extern int32_t zfs_symlink (zfs_fh *dir, string *name, string *to,
			    sattr *attr);
extern int32_t zfs_mknod (zfs_fh *dir, string *name, sattr *attr, ftype type,
			  uint32_t rdev);
extern int32_t refresh_path (zfs_fh *fh);

#endif
