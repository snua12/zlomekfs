/*
   Superblock and inode operations.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/errno.h>

#include "zfs.h"


#if 0
struct address_space_operations {
	int (*writepage)(struct page *page, struct writeback_control *wbc);
	int (*readpage)(struct file *, struct page *);
	int (*sync_page)(struct page *);

	/* Write back some dirty pages from this mapping. */
	int (*writepages)(struct address_space *, struct writeback_control *);

	/* Set a page dirty */
	int (*set_page_dirty)(struct page *page);

	int (*readpages)(struct file *filp, struct address_space *mapping,
			 struct list_head *pages, unsigned nr_pages);

	/*
	 * ext3 requires that a successful prepare_write() call be followed
	 * by a commit_write() call - they must be balanced
	 */
	int (*prepare_write)(struct file *, struct page *, unsigned, unsigned);
	int (*commit_write)(struct file *, struct page *, unsigned, unsigned);
	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
	sector_t (*bmap)(struct address_space *, sector_t);
	int (*invalidatepage) (struct page *, unsigned long);
	int (*releasepage) (struct page *, int);
	int (*direct_IO)(int, struct kiocb *, const struct iovec *iov,
			 loff_t offset, unsigned long nr_segs);
};
#endif

static struct address_space_operations zfs_address_space_operations = {
	.readpage       = simple_readpage,
	.prepare_write  = simple_prepare_write,
	.commit_write   = simple_commit_write
};

#if 0
struct backing_dev_info {
	unsigned long ra_pages; /* max readahead in PAGE_CACHE_SIZE units */
	unsigned long state;    /* Always use atomic bitops on this */
	int memory_backed;      /* Cannot clean pages with writepage */
	congested_fn *congested_fn; /* Function pointer if device is md/dm */
	void *congested_data;   /* Pointer to aux data for congested func */
	void (*unplug_io_fn)(struct backing_dev_info *);
	void *unplug_io_data;
};
#endif

static struct backing_dev_info zfs_backing_dev_info = {
	.ra_pages       = 0,    /* No readahead */
	.memory_backed  = 1,    /* Does not contribute to dirty memory */
};

extern struct inode_operations zfs_file_inode_operations, zfs_dir_inode_operations;
extern struct file_operations zfs_file_operations;

struct inode *zfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (inode) {
		inode->i_mode = mode;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_mapping->a_ops = &zfs_address_space_operations;
		inode->i_mapping->backing_dev_info = &zfs_backing_dev_info;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
			default:
				init_special_inode(inode, mode, dev);
				break;
			case S_IFREG:
				inode->i_op = &zfs_file_inode_operations;
				inode->i_fop = &zfs_file_operations;
				break;
			case S_IFDIR:
				inode->i_op = &zfs_dir_inode_operations;
				inode->i_fop = &simple_dir_operations;

				/* directory inodes start off with i_nlink == 2 (for "." entry) */
				inode->i_nlink++;
				break;
			case S_IFLNK:
				inode->i_op = &page_symlink_inode_operations;
				break;
		}
	}

	return inode;
}

#if 0
/*
 * NOTE: write_inode, delete_inode, clear_inode, put_inode can be called
 * without the big kernel lock held in all filesystems.
 */
struct super_operations {
	struct inode *(*alloc_inode)(struct super_block *sb);
	void (*destroy_inode)(struct inode *);

	void (*read_inode) (struct inode *);

	void (*dirty_inode) (struct inode *);
	void (*write_inode) (struct inode *, int);
	void (*put_inode) (struct inode *);
	void (*drop_inode) (struct inode *);
	void (*delete_inode) (struct inode *);
	void (*put_super) (struct super_block *);
	void (*write_super) (struct super_block *);
	int (*sync_fs)(struct super_block *sb, int wait);
	void (*write_super_lockfs) (struct super_block *);
	void (*unlockfs) (struct super_block *);
	int (*statfs) (struct super_block *, struct kstatfs *);
	int (*remount_fs) (struct super_block *, int *, char *);
	void (*clear_inode) (struct inode *);
	void (*umount_begin) (struct super_block *);

	int (*show_options)(struct seq_file *, struct vfsmount *);
};
#endif

static struct super_operations zfs_super_operations = {
	.drop_inode	= generic_delete_inode,
	.statfs		= simple_statfs,
};

static int zfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root;

/*
	if (!channel.connected) {
		ERROR("zfs: zfsd has not opened communication device\n");
		return -EINVAL;
	}
*/

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = ZFS_MAGIC;
	sb->s_op = &zfs_super_operations;

	/* TODO: zfsd_root_lookup or something */

	root = zfs_get_inode(sb, S_IFDIR | 0755, 0);
	if (!root)
		return -ENOMEM;

	sb->s_root = d_alloc_root(root);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}

static struct super_block *zfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return get_sb_single(fs_type, flags, data, zfs_fill_super);
}

struct file_system_type zfs_type = {
	.owner		= THIS_MODULE,
	.name		= "zfs",
	.get_sb		= zfs_get_sb,
	.kill_sb	= kill_litter_super,
	.fs_flags	= 0,
};

