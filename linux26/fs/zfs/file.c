/*
   File operations.
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

#include <linux/fs.h>
#include <linux/errno.h>

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


static ssize_t zfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
	TRACE("zfs: read\n");

	return 0;
}

static ssize_t zfs_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
	TRACE("zfs: write\n");

	return 0;
}

int zfs_open(struct inode *inode, struct file *file)
{
	zfs_cap *cap;
	open_args args;
	int error;

	TRACE("zfs: open: %p\n", inode);

	cap = kmalloc(sizeof(zfs_cap), GFP_KERNEL);
	if (!cap)
		return -ENOMEM;

	args.file = ZFS_I(inode)->fh;
	args.flags = file->f_flags;

	error = zfsd_open(cap, &args);
	if (error) {
		kfree(cap);
		return error;
	}

	file->private_data = cap;

	return 0;
}

int zfs_release(struct inode *inode, struct file *file)
{
	int error;

	TRACE("zfs: release: %p\n", inode);

	error = zfsd_close(file->private_data);

	kfree(file->private_data);

	return error;
}

struct file_operations zfs_file_operations = {
	.llseek         = generic_file_llseek,
	.read           = zfs_read,
	.write          = zfs_write,
	.open           = zfs_open,
	.release        = zfs_release,
};

