/*
   The main kernel module part.
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
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/semaphore.h>

#include "zfs.h"


MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(ZFS_CHARDEV_MAJOR);

extern struct file_operations zfs_chardev_file_operations;
extern struct file_system_type zfs_type;

struct channel channel;

static int __init zfs_init(void)
{
	int result;

	result = register_chrdev(ZFS_CHARDEV_MAJOR, "zfs", &zfs_chardev_file_operations);
	if (result) {
		ERROR("zfs: unable to register chardev major %d!\n", ZFS_CHARDEV_MAJOR);
		return result;
	}

	/* TODO: add slab for inodes */

	result = register_filesystem(&zfs_type);
	if (result) {
		unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
		ERROR("zfs: unable to register filesystem!\n");
		return result;
	}

	init_MUTEX(&channel.lock);

	return 0;
}

static void __exit zfs_exit(void)
{
	unregister_filesystem(&zfs_type);

	/* TODO: remove slab */

	unregister_chrdev(ZFS_CHARDEV_MAJOR, "zfs");
}

module_init(zfs_init);
module_exit(zfs_exit);

