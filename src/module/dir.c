/*
   Directory operations.
   Copyright (C) 2004 Martin Zlomek

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

#include <linux/fs.h>

#include "zfs.h"
#include "zfs-prot.h"
#include "zfsd-call.h"


static int zfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
        struct inode *inode = file->f_dentry->d_inode;
        read_dir_args args;
        int error;

        TRACE("'%s'", file->f_dentry->d_name.name);

        if (file->f_pos == -1)
                return 0;

        args.cap = *CAP(file->private_data);
        args.cookie = file->f_pos ? *COOKIE(file->private_data) : 0;
        args.count = ZFS_MAXDATA;

        error = zfsd_readdir(&args, file, dirent, filldir);
        if (error == -ESTALE)
                ZFS_I(inode)->flags |= NEED_REVALIDATE;

        return error;
}

extern int zfs_open(struct inode *inode, struct file *file);
extern int zfs_release(struct inode *inode, struct file *file);

struct file_operations zfs_dir_operations = {
        .llseek         = generic_file_llseek,
        .read           = generic_read_dir,
        .readdir        = zfs_readdir,
        .open           = zfs_open,
        .release        = zfs_release,
};
