/*
   ZFSd operations.
   Copyright (C) 2004 Antonin Prukl, Miroslav Rudisin, Martin Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html
 */

#ifndef _ZFSD_CALL_H
#define _ZFSD_CALL_H

#include <linux/fs.h>
#include <linux/compiler.h>

#include "zfs.h"
#include "zfs_prot.h"


extern int send_request(struct request *req);
extern int zfsd_root(zfs_fh *fh);
extern int zfsd_getattr(fattr *attr, zfs_fh *fh);
extern int zfsd_setattr(fattr *attr, sattr_args *args);
extern int zfsd_create(create_res *res, create_args *args);
extern int zfsd_lookup(dir_op_res *res, dir_op_args *args);
extern int zfsd_link(link_args *args);
extern int zfsd_unlink(dir_op_args *args);
extern int zfsd_symlink(dir_op_res *res, symlink_args *args);
extern int zfsd_mkdir(dir_op_res *res, mkdir_args *args);
extern int zfsd_rmdir(dir_op_args *args);
extern int zfsd_mknod(dir_op_res *res, mknod_args *args);
extern int zfsd_rename(rename_args *args);
extern int zfsd_readlink(read_link_res *res, zfs_fh *fh);
extern int zfsd_open(zfs_cap *cap, open_args *args);
extern int zfsd_close(zfs_cap *cap);
extern int zfsd_readdir(read_dir_args *args, struct file *file, void *dirent, filldir_t filldir);
extern int zfsd_read(char __user *buf, read_args *args);
extern int zfsd_write(write_args *args);
extern int zfsd_readpage(char *buf, read_args *args);

#endif
