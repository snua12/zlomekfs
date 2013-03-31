/*! 
 *  \file zfs_dirent_android.c
 *  \brief  Wrapper for POSIX directory operation API for Android platform
 *  \author Ales Snuparek based on Mike Shal <marfey@gmail.com>
 *
 */

/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

 /*
 copied from http://li366-73.members.linode.com/tup/src/compat/fdopendir.c
 */

#include <unistd.h>
#include <dirent.h>
#include "zfs_dirent.h"
#include "linux/linux_dirent.h"


#ifndef __ANDROID__
#error Only for android bionic
#endif

ZFS_DIR * zfs_opendir(const char *dirname)
{
	return getdents_opendir(dirname);
}

static ZFS_DIR *zfs_fdopendir_nofd_dup(int fd)
{
	return getdents_fdopendir(fd);
}

ZFS_DIR * zfs_fdopendir(int fd)
{
	return zfs_fdopendir_nofd_dup(fd);
}

int zfs_closedir(ZFS_DIR *dirp)
{
	return getdents_closedir(dirp);
}

int zfs_readdir_r(ZFS_DIR * dirp, zfs_dirent * entry, struct dirent ** result)
{
	return getdents_readdir_r(dirp, entry, result);
}

void zfs_seekdir(ZFS_DIR *dirp, long loc)
{
	return getdents_seekdir(dirp, loc);
}

long zfs_telldir(ZFS_DIR *dirp)
{
	return getdents_telldir(dirp);
}

