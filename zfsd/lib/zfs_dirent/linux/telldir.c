/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include "dirstream.h"
#include "linux_dirent.h"


long int getdents_telldir(LINUX_DIR * dir)
{
	if (!dir) {
		__set_errno(EBADF);
		return -1;
	}

	/* The next entry. */
	//return dir->dd_nextoff;
	return dir->dd_mynextoff;
}
