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

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


static void zfs_attr_to_iattr(struct inode *inode, fattr *attr)
{
	inode->i_ino = attr->ino;
	inode->i_version = attr->version;
	inode->i_mode = attr->mode | ftype2mode[attr->type];
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

static struct inode *zfs_iget(struct super_block *sb, fattr *attr)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (inode)
		zfs_fill_inode(inode, attr);

	TRACE("zfs: iget: %p\n", inode);

	return inode;
}

int zfs_inode(struct inode **inode, struct super_block *sb, zfs_fh *fh)
{
	fattr attr;
	int error;

	error = zfsd_getattr(&attr, fh);
	if (error)
		return error;

	*inode = zfs_iget(sb, &attr);
	if (!*inode)
		return -ENOMEM;

	return 0;
}

#if 0 /* RAMFS */
static int
ramfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode = ramfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
	}
	return error;
}

static int ramfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	int retval = ramfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		dir->i_nlink++;
	return retval;
}

static int ramfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	return ramfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int ramfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = ramfs_get_inode(dir->i_sb, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			if (dir->i_mode & S_ISGID)
				inode->i_gid = dir->i_gid;
			d_instantiate(dentry, inode);
			dget(dentry);
		} else
			iput(inode);
	}
	return error;
}
#endif

static int zfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	TRACE("zfs: create\n");

	return 0;
}

static struct dentry *zfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	TRACE("zfs: lookup\n");

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

static int zfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
	TRACE("zfs: setattr\n");

	return 0;
}

static int zfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	TRACE("zfs: getattr\n");

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
	.getattr        = zfs_getattr,
};

static struct inode_operations zfs_file_inode_operations = {
	.setattr        = zfs_setattr,
	.getattr        = zfs_getattr,
};

#if 0
struct inode_operations {
	int (*create) (struct inode *,struct dentry *,int, struct nameidata *);
	struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct inode *,struct dentry *,const char *);
	int (*mkdir) (struct inode *,struct dentry *,int);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*mknod) (struct inode *,struct dentry *,int,dev_t);
	int (*rename) (struct inode *, struct dentry *,
		       struct inode *, struct dentry *);
	int (*readlink) (struct dentry *, char __user *,int);
	int (*follow_link) (struct dentry *, struct nameidata *);
	void (*truncate) (struct inode *);
	int (*permission) (struct inode *, int, struct nameidata *);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
	int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*removexattr) (struct dentry *, const char *);
};

struct inode {
	struct hlist_node       i_hash;
	struct list_head        i_list;
	struct list_head        i_dentry;
	unsigned long           i_ino;
	atomic_t                i_count;
	umode_t                 i_mode;
	unsigned int            i_nlink;
	uid_t                   i_uid;
	gid_t                   i_gid;
	dev_t                   i_rdev;
	loff_t                  i_size;
	struct timespec         i_atime;
	struct timespec         i_mtime;
	struct timespec         i_ctime;
	unsigned int            i_blkbits;
	unsigned long           i_blksize;
	unsigned long           i_version;
	unsigned long           i_blocks;
	unsigned short          i_bytes;
	spinlock_t              i_lock; /* i_blocks, i_bytes, maybe i_size */
	struct semaphore        i_sem;
	struct rw_semaphore     i_alloc_sem;
	struct inode_operations *i_op;
	struct file_operations  *i_fop; /* former ->i_op->default_file_ops */
	struct super_block      *i_sb;
	struct file_lock        *i_flock;
	struct address_space    *i_mapping;
	struct address_space    i_data;
	struct dquot            *i_dquot[MAXQUOTAS];
	/* These three should probably be a union */
	struct list_head        i_devices;
	struct pipe_inode_info  *i_pipe;
	struct block_device     *i_bdev;
	struct cdev             *i_cdev;
	int                     i_cindex;

	unsigned long           i_dnotify_mask; /* Directory notify events */
	struct dnotify_struct   *i_dnotify; /* for directory notifications */

	unsigned long           i_state;

	unsigned int            i_flags;
	unsigned char           i_sock;

	atomic_t                i_writecount;
	void                    *i_security;
	__u32                   i_generation;
        union {
		                void            *generic_ip;
				        } u;
#ifdef __NEED_I_SIZE_ORDERED
	        seqcount_t              i_size_seqcount;
#endif
};
#endif
