/*
   Superblock operations.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/semaphore.h>

#include "zfs.h"
#include "zfs-prot.h"
#include "zfsd-call.h"


MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(ZFS_CHARDEV_MAJOR);

struct super_block *zfs_sb;
struct channel channel;

static struct kmem_cache *zfs_inode_cachep;

static struct inode *zfs_alloc_inode(struct super_block *sb)
{
        struct zfs_inode_info *ei;

        ei = kmem_cache_alloc(zfs_inode_cachep, GFP_KERNEL);
        if (!ei)
                return NULL;

        TRACE("%p", &ei->vfs_inode);

        return &ei->vfs_inode;
}

static void zfs_destroy_inode(struct inode *inode)
{
        TRACE("%p", inode);

        kmem_cache_free(zfs_inode_cachep, ZFS_I(inode));
}

static void zfs_init_once(void *foo, struct kmem_cache *cachep,
			  unsigned long flags)
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
        kmem_cache_destroy(zfs_inode_cachep);
}

static void zfs_put_super(struct super_block *sb)
{
        INFO("UMOUNT");

        zfs_sb = NULL;
}

static int zfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
        buf->f_type = ZFS_SUPER_MAGIC;
        buf->f_bsize = ZFS_MAXDATA;
        buf->f_namelen = ZFS_MAXNAMELEN;

        return 0;
}

static struct super_operations zfs_super_operations = {
        .alloc_inode    = zfs_alloc_inode,
        .destroy_inode  = zfs_destroy_inode,
        .put_super	= zfs_put_super,
        .statfs		= zfs_statfs,
};

static int zfs_fill_super(struct super_block *sb, void *data, int silent)
{
        zfs_fh root_fh;
        fattr root_attr;
        struct inode *root_inode;
        int error;

        INFO("MOUNT");

        if (!channel.connected) {
                ERROR("zfsd has not opened communication device!");
                return -EIO;
        }

        sb->s_op = &zfs_super_operations;
        sb->s_magic = ZFS_SUPER_MAGIC;

        /* Get root file handle. */
        error = zfsd_root(&root_fh);
        if (error)
                return error;

        /* Get root inode attributes. */
        error = zfsd_getattr(&root_attr, &root_fh);
        if (error)
                return error;

        /* Make root inode. */
        root_inode = zfs_iget(sb, &root_fh, &root_attr);
        if (!root_inode)
                return -ENOMEM;

        /* Create root dentry. */
        sb->s_root = d_alloc_root(root_inode);
        if (!sb->s_root)
                return -ENOMEM;

        zfs_sb = sb;

        return 0;
}

static int zfs_get_sb(struct file_system_type *fs_type, int flags,
		      const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_single(fs_type, flags, data, zfs_fill_super, mnt);
}

static struct file_system_type zfs_type = {
        .owner		= THIS_MODULE,
        .name		= "zfs",
        .get_sb		= zfs_get_sb,
        .kill_sb	= kill_anon_super,
        .fs_flags	= 0,
};

static int __init zfs_init(void)
{
        int error;

        INFO("INIT");

        /* Register communication device. */
        error = register_chrdev(ZFS_CHARDEV_MAJOR, "zfs", &zfs_chardev_file_operations);
        if (error) {
                ERROR("unable to register chardev major %d!", ZFS_CHARDEV_MAJOR);
                return error;
        }

        /* Initialize inode cache. */
        error = zfs_init_inodecache();
        if (error) {
                unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
                ERROR("unable to create zfs inode cache!");
                return error;
        }

        /* Register ZFS file system. */
        error = register_filesystem(&zfs_type);
        if (error) {
                zfs_destroy_inodecache();
                unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
                ERROR("unable to register filesystem!");
                return error;
        }

        init_MUTEX(&channel.lock);

        return 0;
}

static void __exit zfs_exit(void)
{
        INFO("EXIT");

        unregister_filesystem(&zfs_type);
        zfs_destroy_inodecache();
        unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
}

module_init(zfs_init);
module_exit(zfs_exit);
