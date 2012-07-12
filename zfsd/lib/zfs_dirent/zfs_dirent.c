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

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include "zfs_dirent.h"

#if defined(__APPLE__) && defined(__MACH__)
DIR *zfs_fdopendir(int fd)
{
	char fullpath[MAXPATHLEN];
	DIR *d;
	int rv;
	rv = fcntl(fd, F_GETPATH, fullpath);
	if(rv == -1) {
		perror("fcntl");
		fprintf(stderr, "tup error: Unable to convert file descriptor back to pathname in fdopendir() compat library.\n");
		return NULL;
	}

	d = opendir(fullpath);
	rv = close(fd);
	if (rv == -1)
	{
		perror("close");
	}
	return d;
}

void zfs_seekdir(DIR *dirp, long loc)
{
	seekdir(dirp, 0);
	(void) telldir(dirp);
	long l;
	for (l = 0; l < loc; ++l)
	{
		(void) readdir(dirp);
		(void) telldir(dirp);
	}
}

int zfs_readdir_r(DIR * dirp, struct dirent * entry, struct dirent ** result)
{
	(void) telldir(dirp);
	return readdir_r(dirp, entry, result);
}
#else
DIR *zfs_fdopendir(int fd)
{
	return fdopendir(fd);
}

void zfs_seekdir(DIR *dirp, long loc)
{
	seekdir(dirp, loc);
}

int zfs_readdir_r(DIR * dirp, struct dirent * entry, struct dirent ** result)
{
	return readdir_r(dirp, entry, result);
}
#endif

long zfs_telldir(DIR *dirp)
{
	return telldir(dirp);
}

DIR * zfs_opendir(const char *dirname)
{
	return opendir(dirname);
}

int zfs_closedir(DIR *dirp)
{
	return closedir(dirp);
}

