/*
   File operations.
   Copyright (C) 2004 Martin Zlomek
   Copyright (C) 2004 Josef Zlomek

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
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/pagemap.h>
#include <linux/string.h>

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


static ssize_t zfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
	struct inode *inode = file->f_dentry->d_inode;
	read_args args;
	int error;
	int total = 0;

	TRACE("'%s': %lld", file->f_dentry->d_name.name, *off);

	args.cap = *CAP(file->private_data);
	while (nbytes > 0) {
		args.offset = *off;
		args.count = (nbytes > ZFS_MAXDATA) ? ZFS_MAXDATA : nbytes;

		error = zfsd_read(buf, &args);
		if (error >= 0) {
			*off += error;
			buf += error;
			total += error;
			nbytes -= error;
			inode->i_atime = CURRENT_TIME;
			if (*off > i_size_read(inode)) {
				i_size_write(inode, *off);
				inode->i_ctime = CURRENT_TIME;
			}
			if (error < args.count)
				break;
		} else {
			if (error == -ESTALE)
				ZFS_I(inode)->flags |= NEED_REVALIDATE;
			return error;
		}
	}

	return total;
}

static ssize_t zfs_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
	struct inode *inode = file->f_dentry->d_inode;
	write_args args;
	int error;
	int total = 0;

	TRACE("'%s': %lld", file->f_dentry->d_name.name, *off);

	args.cap = *CAP(file->private_data);
	while (nbytes > 0) {
		args.offset = (file->f_flags & O_APPEND) ? i_size_read(inode) : *off;
		args.data.len = (nbytes > ZFS_MAXDATA) ? ZFS_MAXDATA : nbytes;
		args.data.buf = buf;

		error = zfsd_write(&args);
		if (error >= 0) {
			*off = args.offset + error;
			buf += error;
			total += error;
			nbytes -= error;
			inode->i_mtime = CURRENT_TIME;
			if (*off > i_size_read(inode)) {
				i_size_write(inode, *off);
				inode->i_ctime = CURRENT_TIME;
			}
			if (error < args.data.len)
				break;
		} else {
			if (error == -ESTALE)
				ZFS_I(inode)->flags |= NEED_REVALIDATE;
			return error;
		}
	}

	return total;
}

int zfs_open(struct inode *inode, struct file *file)
{
	struct dentry *dentry = file->f_dentry;
	zfs_cap *cap;
	open_args args;
	int error;

	TRACE("'%s'", dentry->d_name.name);

	if ((file->f_flags & O_CREAT) && dentry->d_fsdata) {
		/* We already have CAP for the file (zfs_create() has found it out). */
		file->private_data = dentry->d_fsdata;
		dentry->d_fsdata = NULL;
	} else {
		cap = kmalloc(sizeof(zfs_cap) + (S_ISDIR(inode->i_mode) ? sizeof(int32_t) : 0), GFP_KERNEL);
		if (!cap)
			return -ENOMEM;

		args.file = ZFS_I(inode)->fh;
		args.flags = file->f_flags;

		error = zfsd_open(cap, &args);
		if (error) {
			kfree(cap);
			if (error == -ESTALE)
				ZFS_I(inode)->flags |= NEED_REVALIDATE;
			return error;
		}

		file->private_data = cap;
	}

	return 0;
}

int zfs_release(struct inode *inode, struct file *file)
{
	int error;

	TRACE("'%s'", file->f_dentry->d_name.name);

	error = zfsd_close(CAP(file->private_data));

	kfree(file->private_data);

	return error;
}

static int zfs_readpage(struct file *file, struct page *page)
{
	char *kaddr;
	read_args args;
	int error = 0;

	TRACE("'%s': %lu", file->f_dentry->d_name.name, page->index);

	if (PageUptodate(page))
		goto out;

	args.cap = *CAP(file->private_data);
	args.offset = page->index << PAGE_CACHE_SHIFT;
	args.count = PAGE_CACHE_SIZE;

	kaddr = kmap(page);

	error = zfsd_readpage(kaddr, &args);
	if (error >= 0) {
		if (error < PAGE_CACHE_SIZE)
			/* Zero the rest of the page. */
			memset(kaddr + error, 0, PAGE_CACHE_SIZE - error);

		SetPageUptodate(page);

		error = 0;
	} else if (error == -ESTALE)
		ZFS_I(file->f_dentry->d_inode)->flags |= NEED_REVALIDATE;

	kunmap(page);

out:
	unlock_page(page);

	return error;
}

struct file_operations zfs_file_operations = {
	.llseek         = generic_file_llseek,
	.read           = zfs_read,
	.write          = zfs_write,
	.mmap		= generic_file_readonly_mmap,
	.open           = zfs_open,
	.release        = zfs_release,
};

struct address_space_operations zfs_file_address_space_operations = {
	.readpage           = zfs_readpage,
};
