/*! \file \brief Directory tests*/

/* Copyright (C) 2008, 2012 Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "syscall_collector.h"
#ifdef __CYGWIN__
#include <Windows.h>
#include <sys/cygwin.h>
#endif


#include "dir_tests.h"

#define TEST_FILE_1 "a.txt"
#define TEST_FILE_2 "b.txt"

void generate_directory_content(char * path, int count, int deep)
{
	char local_path[PATH_MAX + 1];
	strncpy(local_path, path, PATH_MAX - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		//CreateDirectory
		get_filename(local_path + local_path_len);
		collect(SYSCALL_OP_MKDIR, SYSCALL_STATE_BEGIN);
		int rv = mkdir(local_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		collect(SYSCALL_OP_MKDIR, SYSCALL_STATE_END);
		if (rv == 0)
		{
			int l = strlen(local_path);
			local_path[l] = '/';
			local_path[l+1] = 0;

			if (deep > 0)
			{
				generate_directory_content(local_path, count, deep - 1);
			}
			else
			{
				generate_file_content(local_path, count * 2);
			}
		}
		else
		{

			printf("%s:%d \"%s\" last error is %u %x %s\n", __func__, __LINE__,
				local_path, errno, errno, strerror(errno));
		}
	}
}

void cleanup_directory_content(char * path, int count, int deep)
{

	char local_path[PATH_MAX + 1];
	strncpy(local_path, path, PATH_MAX - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		get_filename(local_path + local_path_len);
		int l = strlen(local_path);
		local_path[l] = '/';
		local_path[l+1] = 0;

		if (deep > 0)
		{
			cleanup_directory_content(local_path, count, deep - 1);

		}
		else
		{
			cleanup_file_content(local_path, count * 2);
		}

		local_path[l] = 0;

#ifdef __CYGWIN__
		/*this is Dokan workaround delete directory, when is called unlink_nt with some params,
		 delete file is called instead of delete directory.*/
		char windows_path[MAX_PATH + 1]; 
		cygwin_conv_path (CCP_POSIX_TO_WIN_A, local_path, windows_path, MAX_PATH);

		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_BEGIN);
		BOOL rv = RemoveDirectory(windows_path);
		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_END);
		if (rv == 0)
		{
			printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
				local_path, GetLastError(), GetLastError());
		}
#else

		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_BEGIN);
		int rv = rmdir(local_path);
		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_END);
		if (rv == -1)
		{
			printf("%s:%d \"%s\" last error is %u %x %s\n", __func__, __LINE__,
				local_path, errno, errno, strerror(errno));
		}
#endif
	}
}


