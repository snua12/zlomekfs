/*
   Superblock operations.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <asm/semaphore.h>

#include "zfs.h"
#include "zfs_prot.h"
#include "zfsd_call.h"


MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(ZFS_CHARDEV_MAJOR);

struct channel channel;

extern struct file_operations zfs_chardev_file_operations;

static kmem_cache_t *zfs_inode_cachep;

static struct inode *zfs_alloc_inode(struct super_block *sb)
{
	struct zfs_inode_info *ei;

	ei = kmem_cache_alloc(zfs_inode_cachep, SLAB_KERNEL);
	if (!ei)
		return NULL;

	TRACE("zfs: alloc_inode: %p\n", &ei->vfs_inode);

	return &ei->vfs_inode;
}

static void zfs_destroy_inode(struct inode *inode)
{
	TRACE("zfs: destroy_inode: %p\n", inode);

	kmem_cache_free(zfs_inode_cachep, ZFS_I(inode));
}

static void zfs_init_once(void *foo, kmem_cache_t *cachep, unsigned long flags)
{
	struct zfs_inode_info *ei = (struct zfs_inode_info *)foo;

	if ((flags & (SLAB_CTOR_VERIFY | SLAB_CTOR_CONSTRUCTOR)) == SLAB_CTOR_CONSTRUCTOR)
		inode_init_once(&ei->vfs_inode);
}

static int zfs_init_inodecache(void)
{
	zfs_inode_cachep = kmem_cache_create("zfs_inode_cache",
					     sizeof(struct zfs_inode_info),
					     0,
					     SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT,
					     zfs_init_once,
					     NULL);
	if (!zfs_inode_cachep)
		return -ENOMEM;

	return 0;
}

static void zfs_destroy_inodecache(void)
{
	if (kmem_cache_destroy(zfs_inode_cachep))
		INFO("zfs_inode_cache: not all structures were freed\n");
}


static struct super_operations zfs_super_operations = {
	.alloc_inode    = zfs_alloc_inode,
	.destroy_inode  = zfs_destroy_inode,
	.statfs		= simple_statfs,
};

extern int zfs_inode(struct inode **inode, struct super_block *sb, zfs_fh *fh);

static int zfs_fill_super(struct super_block *sb, void *data, int silent)
{
	zfs_fh root_fh;
	struct inode *root_inode;
	int error;

	TRACE("zfs: fill_super\n");

	if (!channel.connected) {
		ERROR("zfs: zfsd has not opened communication device\n");
		return -ECOMM;
	}

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = ZFS_MAGIC;
	sb->s_op = &zfs_super_operations;

	error = zfsd_root(&root_fh);
	if (error)
		return error;

	error = zfs_inode(&root_inode, sb, &root_fh);
	if (error)
		return error;

	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}

static struct super_block *zfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return get_sb_single(fs_type, flags, data, zfs_fill_super);
}

static struct file_system_type zfs_type = {
	.owner		= THIS_MODULE,
	.name		= "zfs",
	.get_sb		= zfs_get_sb,
	.kill_sb	= kill_litter_super,
	.fs_flags	= 0,
};

static int __init zfs_init(void)
{
	int error;

	error = register_chrdev(ZFS_CHARDEV_MAJOR, "zfs", &zfs_chardev_file_operations);
	if (error) {
		ERROR("zfs: unable to register chardev major %d!\n", ZFS_CHARDEV_MAJOR);
		return error;
	}

	error = zfs_init_inodecache();
	if (error) {
		unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
		ERROR("zfs: unable to create zfs inode cache\n");
		return error;
	}

	error = register_filesystem(&zfs_type);
	if (error) {
		zfs_destroy_inodecache();
		unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
		ERROR("zfs: unable to register filesystem!\n");
		return error;
	}

	init_MUTEX(&channel.lock);

	return 0;
}

static void __exit zfs_exit(void)
{
	unregister_filesystem(&zfs_type);
	zfs_destroy_inodecache();
	unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
}

module_init(zfs_init);
module_exit(zfs_exit);

