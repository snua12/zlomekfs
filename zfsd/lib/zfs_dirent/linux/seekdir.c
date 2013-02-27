/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include "dirstream.h"
#include "linux_dirent.h"


void getdents_seekdir(LINUX_DIR * dir, long int offset)
{
	if (!dir) {
		__set_errno(EBADF);
		return;
	}
	__UCLIBC_MUTEX_LOCK(dir->dd_lock);
	dir->dd_nextoff = lseek(dir->dd_fd, 0, SEEK_SET);
	dir->dd_size = dir->dd_nextloc = 0;
	__UCLIBC_MUTEX_UNLOCK(dir->dd_lock);

	int i;
	struct dirent * de = NULL;
	struct dirent entry;
	int rv;
	for (i = 0; i < offset; ++i)
	{
		rv = getdents_readdir_r(dir, &entry, &de);
		if (rv != 0)
		{
			__set_errno(EBADF);
			break;
		}

		if (de == NULL)
		{
			__set_errno(EBADF);
			break;
		}
	}
}
