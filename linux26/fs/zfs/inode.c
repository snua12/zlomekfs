/*
   Inode operations.
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
#include <linux/kdev_t.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/stat.h>

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


static void zfs_attr_to_iattr(struct inode *inode, fattr *attr)
{
	inode->i_ino = attr->ino;
	inode->i_version = attr->version;
	inode->i_mode = ftype2mode[attr->type] | attr->mode;
	inode->i_nlink = attr->nlink;
	inode->i_uid = attr->uid;
	inode->i_gid = attr->gid;
	inode->i_rdev = attr->rdev;
	inode->i_size = attr->size;
	inode->i_blocks = attr->blocks;
	inode->i_blksize = attr->blksize;
	inode->i_atime.tv_sec = attr->atime;
	inode->i_mtime.tv_sec = attr->mtime;
	inode->i_ctime.tv_sec = attr->ctime;
}

static struct inode_operations zfs_file_inode_operations, zfs_dir_inode_operations;
extern struct file_operations zfs_file_operations, zfs_dir_operations;

static void zfs_fill_inode(struct inode *inode, fattr *attr)
{
	zfs_attr_to_iattr(inode, attr);
	switch (inode->i_mode & S_IFMT) {
		case S_IFREG:
			inode->i_op = &zfs_file_inode_operations;
			inode->i_fop = &zfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &zfs_dir_inode_operations;
			inode->i_fop = &zfs_dir_operations;
			break;
		case S_IFLNK:
//			inode->i_op = &zfs_symlink_inode_operations;
//			inode->i_data.a_ops = &zfs_symlink_aops;
//			inode->i_mapping = &inode->i_data;
			break;
		default:
			init_special_inode(inode, inode->i_mode, huge_decode_dev(inode->i_rdev));
			break;
	}
}

static int zfs_test_inode(struct inode *inode, void *data)
{
	return !memcmp(&ZFS_I(inode)->fh, data, sizeof(zfs_fh));
}

static int zfs_set_inode(struct inode *inode, void *data)
{
	ZFS_I(inode)->fh = *(zfs_fh *)data;

	return 0;
}

struct inode *zfs_iget(struct super_block *sb, zfs_fh *fh, fattr *attr)
{
	struct inode *inode;

	TRACE("zfs: iget: %u\n", fh->ino);

	inode = iget5_locked(sb, HASH(fh), zfs_test_inode, zfs_set_inode, fh);

	if (inode && (inode->i_state & I_NEW)) {
		zfs_fill_inode(inode, attr);
		unlock_new_inode(inode);
	}

	return inode;
}

static int zfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	TRACE("zfs: create\n");

	return 0;
}

static struct dentry *zfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	dir_op_args args;
	dir_op_res res;
	struct inode *inode;
	int error;

	TRACE("zfs: lookup: '%s'\n", dentry->d_name.name);

	args.dir = ZFS_I(dir)->fh;
	args.name.str = (char *)dentry->d_name.name;
	args.name.len = dentry->d_name.len;

	error = zfsd_lookup(&res, &args);
	if (error)
		return ERR_PTR(error);

	inode = zfs_iget(dir->i_sb, &res.file, &res.attr);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	d_instantiate(dentry, inode);

	return NULL;
}

static int zfs_link(struct dentry *src_dentry, struct inode *dir, struct dentry *dst_dentry)
{
	TRACE("zfs: link\n");

	return 0;
}

static int zfs_unlink(struct inode *dir, struct dentry *dentry)
{
	TRACE("zfs: unlink\n");

	return 0;
}

static int zfs_symlink(struct inode *dir, struct dentry *dentry, const char *old_name)
{
	TRACE("zfs: symlink\n");

	return 0;
}

static int zfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	TRACE("zfs: mkdir\n");

	return 0;
}

static int zfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	TRACE("zfs: rmdir\n");

	return 0;
}

static int zfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	TRACE("zfs: mknod\n");

	return 0;
}

static int zfs_rename (struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	TRACE("zfs: rename\n");

	return 0;
}

static void zfs_iattr_to_sattr(sattr *attr, struct iattr *iattr)
{
	attr->mode = iattr->ia_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
	attr->uid = iattr->ia_uid;
	attr->gid = iattr->ia_gid;
	attr->size = iattr->ia_size;
	attr->atime = iattr->ia_atime.tv_sec;
	attr->mtime = iattr->ia_mtime.tv_sec;
}

static int zfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
	struct inode *inode = dentry->d_inode;
	fattr attr;
	sattr_args args;
	int error;

	TRACE("zfs: setattr: '%s'\n", dentry->d_name.name);

	args.file = ZFS_I(inode)->fh;
	zfs_iattr_to_sattr(&args.attr, iattr);

	error = zfsd_setattr(&attr, &args);

	if (!error) {
		inode->i_ctime = CURRENT_TIME;
		zfs_attr_to_iattr(inode, &attr);
	}

	return error;
}

static struct inode_operations zfs_dir_inode_operations = {
	.create         = zfs_create,
	.lookup         = zfs_lookup,
	.link           = zfs_link,
	.unlink         = zfs_unlink,
	.symlink        = zfs_symlink,
	.mkdir          = zfs_mkdir,
	.rmdir          = zfs_rmdir,
	.mknod          = zfs_mknod,
	.rename         = zfs_rename,
	.setattr        = zfs_setattr,
};

static struct inode_operations zfs_file_inode_operations = {
	.setattr        = zfs_setattr,
};

