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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/time.h>

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


static void zfs_iattr_to_sattr(sattr *attr, struct iattr *iattr)
{
	unsigned int valid = iattr->ia_valid;

	attr->mode = (valid & ATTR_MODE) ? (iattr->ia_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX)) : -1;
	attr->uid = (valid & ATTR_UID) ? iattr->ia_uid : -1;
	attr->gid = (valid & ATTR_GID) ? iattr->ia_gid : -1;
	attr->size = (valid & ATTR_SIZE) ? iattr->ia_size : -1;
	attr->atime = (valid & ATTR_ATIME) ? iattr->ia_atime.tv_sec : -1;
	attr->mtime = (valid & ATTR_MTIME) ? iattr->ia_mtime.tv_sec : -1;
}

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
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = attr->mtime;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = attr->ctime;
	inode->i_ctime.tv_nsec = 0;
}

static ftype zfs_mode_to_ftype(int mode)
{
	switch (mode & S_IFMT) {
		case S_IFSOCK:
			return FT_SOCK;
		case S_IFLNK:
			return FT_LNK;
		case S_IFREG:
			return FT_REG;
		case S_IFBLK:
			return FT_BLK;
		case S_IFDIR:
			return FT_DIR;
		case S_IFCHR:
			return FT_CHR;
		case S_IFIFO:
			return FT_FIFO;
		default:
			return FT_BAD;
	}
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
	create_args args;
	create_res res;
	struct inode *inode;
	int error;

	TRACE("zfs: create: '%s'\n", dentry->d_name.name);

	args.where.dir = ZFS_I(dir)->fh;
	args.where.name.str = (char *)dentry->d_name.name;
	args.where.name.len = dentry->d_name.len;
	args.flags = nd->intent.open.flags;
	args.attr.mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
	args.attr.uid = current->fsuid;
	if (dir->i_mode & S_ISGID)
		args.attr.gid = dir->i_gid;
	else
		args.attr.gid = current->fsgid;
	args.attr.size = -1;
	args.attr.atime = -1;
	args.attr.mtime = -1;

	error = zfsd_create(&res, &args);
	if (error)
		return error;

	/* FIXME: how to give CAPs to ->open(), when 2 threads create the same file in the same time (they share the same dentry) */
	dentry->d_fsdata = kmalloc(sizeof(zfs_cap), GFP_KERNEL);
	if (!dentry->d_fsdata)
		return -ENOMEM;
	*(zfs_cap *)dentry->d_fsdata = res.cap;

	inode = zfs_iget(dir->i_sb, &res.file, &res.attr);
	if (!inode)
		return -ENOMEM;

	d_instantiate(dentry, inode);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static struct dentry *zfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	dir_op_args args;
	dir_op_res res;
	struct inode *inode;
	int error;

	TRACE("zfs: lookup: '%s'\n", dentry->d_name.name);

	if (dentry->d_name.len > ZFS_MAXNAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	args.dir = ZFS_I(dir)->fh;
	args.name.str = (char *)dentry->d_name.name;
	args.name.len = dentry->d_name.len;

	error = zfsd_lookup(&res, &args);
	if (error) {
		if (error == -ENOENT) {
			d_add(dentry, NULL);
			return NULL;
		}
		return ERR_PTR(error);
	}

	inode = zfs_iget(dir->i_sb, &res.file, &res.attr);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	d_add(dentry, inode);

	return NULL;
}

static int zfs_link(struct dentry *src_dentry, struct inode *dir, struct dentry *dst_dentry)
{
	struct inode *inode = src_dentry->d_inode;
	link_args args;
	int error;

	TRACE("zfs: link: '%s' -> '%s'\n", dst_dentry->d_name.name, src_dentry->d_name.name);

	args.from = ZFS_I(inode)->fh;
	args.to.dir = ZFS_I(dir)->fh;
	args.to.name.str = (char *)dst_dentry->d_name.name;
	args.to.name.len = dst_dentry->d_name.len;

	error = zfsd_link(&args);
	if (error)
		return error;

	inode->i_nlink++;
	inode->i_ctime = CURRENT_TIME;

	atomic_inc(&inode->i_count);
	d_instantiate(dst_dentry, inode);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static int zfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	dir_op_args args;
	int error;

	TRACE("zfs: unlink: '%s'\n", dentry->d_name.name);

	args.dir = ZFS_I(dir)->fh;
	args.name.str = (char *)dentry->d_name.name;
	args.name.len = dentry->d_name.len;

	error = zfsd_unlink(&args);
	if (error)
		return error;

	inode->i_nlink--;
	inode->i_ctime = CURRENT_TIME;

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static int zfs_symlink(struct inode *dir, struct dentry *dentry, const char *old_name)
{
	TRACE("zfs: symlink\n");

	return 0;
}

static int zfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	mkdir_args args;
	dir_op_res res;
	struct inode *inode;
	int error;

	TRACE("zfs: mkdir: '%s'\n", dentry->d_name.name);

	args.where.dir = ZFS_I(dir)->fh;
	args.where.name.str = (char *)dentry->d_name.name;
	args.where.name.len = dentry->d_name.len;
	args.attr.mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
	args.attr.uid = current->fsuid;
	if (dir->i_mode & S_ISGID) {
		args.attr.gid = dir->i_gid;
		args.attr.mode |= S_ISGID;
	} else
		args.attr.gid = current->fsgid;
	args.attr.size = -1;
	args.attr.atime = -1;
	args.attr.mtime = -1;

	error = zfsd_mkdir(&res, &args);
	if (error)
		return error;

	inode = zfs_iget(dir->i_sb, &res.file, &res.attr);
	if (!inode)
		return -ENOMEM;

	d_instantiate(dentry, inode);

	dir->i_nlink++;
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static int zfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	dir_op_args args;
	int error;

	TRACE("zfs: rmdir: '%s'\n", dentry->d_name.name);

	args.dir = ZFS_I(dir)->fh;
	args.name.str = (char *)dentry->d_name.name;
	args.name.len = dentry->d_name.len;

	error = zfsd_rmdir(&args);
	if (error)
		return error;

	inode->i_nlink--;

	dir->i_nlink--;
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static int zfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t rdev)
{
	mknod_args args;
	dir_op_res res;
	struct inode *inode;
	int error;

	TRACE("zfs: mknod: '%s'\n", dentry->d_name.name);

	args.where.dir = ZFS_I(dir)->fh;
	args.where.name.str = (char *)dentry->d_name.name;
	args.where.name.len = dentry->d_name.len;
	args.attr.mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
	args.attr.uid = current->fsuid;
	args.attr.gid = current->fsgid;
	args.attr.size = -1;
	args.attr.atime = -1;
	args.attr.mtime = -1;
	args.type = zfs_mode_to_ftype(mode);
	args.rdev = huge_encode_dev(rdev);

	error = zfsd_mknod(&res, &args);
	if (error)
		return error;

	inode = zfs_iget(dir->i_sb, &res.file, &res.attr);
	if (!inode)
		return -ENOMEM;

	d_instantiate(dentry, inode);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

static int zfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	rename_args args;
	int error;

	TRACE("zfs: rename: '%s' -> '%s'\n", old_dentry->d_name.name, new_dentry->d_name.name);

	args.from.dir = ZFS_I(old_dir)->fh;
	args.from.name.str = (char *)old_dentry->d_name.name;
	args.from.name.len = old_dentry->d_name.len;
	args.to.dir = ZFS_I(new_dir)->fh;
	args.to.name.str = (char *)new_dentry->d_name.name;
	args.to.name.len = new_dentry->d_name.len;

	error = zfsd_rename(&args);
	if (error)
		return error;

	if (S_ISDIR(old_dentry->d_inode->i_mode)) {
		old_dir->i_nlink--;
		new_dir->i_nlink++;
	}
	old_dir->i_mtime = old_dir->i_ctime = CURRENT_TIME;
	new_dir->i_mtime = new_dir->i_ctime = CURRENT_TIME;

	return 0;
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
	if (error)
		return error;

	zfs_attr_to_iattr(inode, &attr);

	return 0;
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

