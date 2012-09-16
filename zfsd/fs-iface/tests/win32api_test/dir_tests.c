/* ! \file \brief Directory tests*/

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek

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
#include <Windows.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "syscall_collector.h"


#include "dir_tests.h"

#define TEST_FILE_1 "a.txt"
#define TEST_FILE_2 "b.txt"

void test_move_file(const char * path)
{
	char path1[MAX_PATH];
	char path2[MAX_PATH];

	sprintf(path1, "%s\\%s", path, TEST_FILE_1); 
	sprintf(path2, "%s\\%s", path, TEST_FILE_2); 

	create_test_file(path1);
	create_test_file(path2);

	BOOL status = MoveFileEx(path1, path2, MOVEFILE_REPLACE_EXISTING);
	if (status == FALSE)
	{
		printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
			path2, GetLastError(), GetLastError());
	}

	create_test_file(path1);
	status = MoveFile(path1, path2);
	if (status == FALSE && GetLastError() == ERROR_ALREADY_EXISTS)
	{
	}
	else
	{
		printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
			path2, GetLastError(), GetLastError());
	}

}

void cleanup_move_file(const char * path)
{
	return ;

	char path1[MAX_PATH];
	char path2[MAX_PATH];

	sprintf(path1, "%s\\%s", path, TEST_FILE_1); 
	sprintf(path2, "%s\\%s", path, TEST_FILE_2); 

	DeleteFile(path1);
	DeleteFile(path2);


}

void generate_directory_content(char * path, int count, int deep)
{
	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		//CreateDirectory
		get_filename(local_path + local_path_len);
		collect(SYSCALL_OP_MKDIR, SYSCALL_STATE_BEGIN);
		BOOL rv = CreateDirectory(local_path, NULL);
		collect(SYSCALL_OP_MKDIR, SYSCALL_STATE_END);
		if (rv)
		{
			int l = strlen(local_path);
			local_path[l] = '\\';
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

			printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
				local_path, GetLastError(), GetLastError());
		}
	}
}

void cleanup_directory_content(char * path, int count, int deep)
{

	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		get_filename(local_path + local_path_len);
		int l = strlen(local_path);
		local_path[l] = '\\';
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

		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_BEGIN);
		BOOL rv = RemoveDirectory(local_path);
		collect(SYSCALL_OP_RMDIR, SYSCALL_STATE_END);
		if (rv == 0)
		{
			printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
				local_path, GetLastError(), GetLastError());
		}
	}
}


