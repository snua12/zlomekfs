/* Directory operations.
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

#ifndef DIR_H
#define DIR_H

#include "system.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"
#include "zfs_prot.h"
#include "metadata.h"

extern void build_local_path (string *dst, volume vol, internal_dentry dentry);
extern void build_local_path_name (string *dst, volume vol,
				   internal_dentry dentry, string *name);
extern void build_relative_path (string *dst, internal_dentry dentry);
extern void file_name_from_path (string *dst, string *path);
extern int32_t recursive_unlink (string *path, uint32_t vid,
				 bool destroy_dentry);
extern int32_t validate_operation_on_virtual_directory (virtual_dir pvd,
							string *name,
							internal_dentry *dir,
							uint32_t conflict_error);
extern int32_t validate_operation_on_zfs_fh (zfs_fh *fh,
					     uint32_t conflict_error,
					     uint32_t non_exist_error);
extern int32_t validate_operation_on_volume_root (internal_dentry dentry,
						  uint32_t conflict_error);
extern int32_t get_volume_root_remote (volume vol, zfs_fh *remote_fh,
				       fattr *attr);
extern int32_t get_volume_root_dentry (volume vol, internal_dentry *dentryp,
				       bool unlock_fh_mutex);
extern int32_t local_getattr_path (fattr *attr, string *path);
extern int32_t local_getattr (fattr *attr, internal_dentry dentry, volume vol);
extern int32_t remote_getattr (fattr *attr, internal_dentry dentry, volume vol);
extern int32_t zfs_getattr (fattr *fa, zfs_fh *fh);
extern int32_t local_setattr_path (fattr *fa, string *path, sattr *sa);
extern int32_t local_setattr (fattr *fa, internal_dentry dentry, sattr *sa,
			      volume vol);
extern int32_t remote_setattr (fattr *fa, internal_dentry dentry, sattr *sa,
			       volume vol);
extern int32_t zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa);
extern int32_t zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path);
extern int32_t local_lookup (dir_op_res *res, internal_dentry dir,
			     string *name, volume vol, metadata *meta);
extern int32_t remote_lookup (dir_op_res *res, internal_dentry dir,
			      string *name, volume vol);
extern int32_t zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name);
extern int32_t local_mkdir (dir_op_res *res, internal_dentry dir, string *name,
			    sattr *attr, volume vol, metadata *meta);
extern int32_t remote_mkdir (dir_op_res *res, internal_dentry dir, string *name,
			     sattr *attr, volume vol);
extern int32_t zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name,
			  sattr *attr);
extern int32_t zfs_rmdir (zfs_fh *dir, string *name);
extern int32_t zfs_rename (zfs_fh *from_dir, string *from_name,
			   zfs_fh *to_dir, string *to_name);
extern int32_t zfs_link (zfs_fh *from, zfs_fh *dir, string *name);
extern int32_t zfs_unlink (zfs_fh *dir, string *name);
extern int32_t local_readlink (read_link_res *res, internal_dentry file,
			       volume vol);
extern int32_t local_readlink_name (read_link_res *res, internal_dentry dir,
				    string *name, volume vol);
extern int32_t remote_readlink (read_link_res *res, internal_dentry file,
				volume vol);
extern int32_t remote_readlink_zfs_fh (read_link_res *res, zfs_fh *fh,
				       volume vol);
extern int32_t zfs_readlink (read_link_res *res, zfs_fh *fh);
extern int32_t local_symlink (dir_op_res *res, internal_dentry dir,
			      string *name, string *to, sattr *attr,
			      volume vol, metadata *meta);
extern int32_t remote_symlink (dir_op_res *res, internal_dentry dir,
			       string *name, string *to, sattr *attr,
			       volume vol);
extern int32_t zfs_symlink (dir_op_res *res, zfs_fh *dir, string *name,
			    string *to, sattr *attr);
extern int32_t local_mknod (dir_op_res *res, internal_dentry dir, string *name,
			    sattr *attr, ftype type, uint32_t rdev, volume vol,
			    metadata *meta);
extern int32_t remote_mknod (dir_op_res *res, internal_dentry dir, string *name,
			     sattr *attr, ftype type, uint32_t rdev,
			     volume vol);
extern int32_t zfs_mknod (dir_op_res *res, zfs_fh *dir, string *name,
			  sattr *attr, ftype type, uint32_t rdev);
extern int32_t local_file_info (file_info_res *res, zfs_fh *fh, volume vol);
extern int32_t remote_file_info (file_info_res *res, zfs_fh *fh, volume vol);
extern int32_t zfs_file_info (file_info_res *res, zfs_fh *fh);
extern int32_t local_reintegrate_add (volume vol, internal_dentry dir,
				      string *name, zfs_fh *fh);
extern int32_t remote_reintegrate_add (volume vol, internal_dentry dir,
				       string *name, zfs_fh *fh);
extern int32_t zfs_reintegrate_add (zfs_fh *fh, zfs_fh *dir, string *name);
extern int32_t local_reintegrate_del_base (zfs_fh *fh, string *name,
					   bool destroy_p, zfs_fh *dir_fh);
extern int32_t local_reintegrate_del (volume vol, zfs_fh *fh,
				      internal_dentry dir, string *name,
				      bool destroy_p, zfs_fh *dir_fh);
extern int32_t remote_reintegrate_del (volume vol, zfs_fh *fh,
				       internal_dentry dir, string *name,
				       bool destroy_p);
extern int32_t zfs_reintegrate_del (zfs_fh *fh, zfs_fh *dir, string *name,
				    bool destroy_p);
extern int32_t local_reintegrate_set (internal_dentry dentry, uint64_t version,
				      volume vol);
extern int32_t remote_reintegrate_set (internal_dentry dentry, uint64_t version,
				       zfs_fh *fh, volume vol);
extern int32_t zfs_reintegrate_set (zfs_fh *fh, uint64_t version);
extern int32_t local_invalidate_fh (zfs_fh *fh);
extern int32_t local_invalidate (internal_dentry dentry, bool volume_root_p);
extern int32_t refresh_fh (zfs_fh *fh);
extern int32_t refresh_local_path (volume vol, string *local_path);

#endif
