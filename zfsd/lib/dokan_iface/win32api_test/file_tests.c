/* ! \file \brief File tests*/

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

#define TEST_PATTERN "TEST STRING 123456789\n"

static void write_test_pattern(HANDLE h)
{

	DWORD bytes_written;
	char text[] = TEST_PATTERN;
	WriteFile(h, text, sizeof(text) - 1, &bytes_written, NULL);
	if (bytes_written != (sizeof(text) - 1))

	{
		printf("WriteFile has failed, last error is %lu %lx\n", GetLastError(), GetLastError());
	}
}

void test_file_op(char * path)
{
	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	get_filename(local_path + local_path_len);
	HANDLE h = CreateFile(local_path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (h == INVALID_HANDLE_VALUE)
	{
		printf("%s:%d last error is %lu %lx\n", __func__, __LINE__,
			GetLastError(), GetLastError());
		return;
	}

	write_test_pattern(h);
	CloseHandle(h);

	h = CreateFile(local_path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (h == INVALID_HANDLE_VALUE)
	{
		printf("last error is %ld\n", GetLastError());
		return;
	}

	DWORD bytes_read;
	const char text[] = TEST_PATTERN;
	char read_text[sizeof(text)];
	ReadFile(h, read_text, sizeof(text) - 1, &bytes_read, NULL);
	if (strcmp(read_text, text) == 0)
	{
		printf("read text is OK\n");
	}
	CloseHandle(h);

	h = CreateFile(local_path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (h == INVALID_HANDLE_VALUE)
	{
		printf("last error is %ld\n", GetLastError());
		return;
	}

	BOOL rv;
	rv = SetEndOfFile(h);
	if (rv == FALSE)
	{
		printf("failed SetEndOfFile, last error is %ld\n", GetLastError());
	}

	CloseHandle(h);

	h = CreateFile(local_path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (h == INVALID_HANDLE_VALUE)
	{
		printf("last error is %ld\n", GetLastError());
		return;
	}

	write_test_pattern(h);

	CloseHandle(h);

	h = CreateFile(local_path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);


	DWORD where = SetFilePointer(h, 1024 * 1024, NULL, FILE_BEGIN);
	if (where == 0)
	{
		printf("failed SetEndOfFile, last error is %ld\n", GetLastError());
	}

	rv = SetEndOfFile(h);
	if (rv == FALSE)
	{
		printf("failed SetEndOfFile, last error is %ld\n", GetLastError());
	}

	CloseHandle(h);
}

void cleanup_file_op(char * path)
{
	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	get_filename(local_path + local_path_len);
	BOOL rv = DeleteFile(local_path);
	if (rv == 0)
	{
		printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
			local_path, GetLastError(), GetLastError());
	}
}

void create_test_file(char * path)
{
	HANDLE h = CreateFile(path,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (h == INVALID_HANDLE_VALUE)
	{
		printf("last error is %ld\n", GetLastError());
	}

	write_test_pattern(h);
	CloseHandle(h);

}


void generate_file_content(char * path, int count)
{
	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		get_filename(local_path + local_path_len);
		create_test_file(local_path);
	}
}


void cleanup_file_content(char * path, int count)
{
	char local_path[MAX_PATH + 1];
	strncpy(local_path, path, MAX_PATH - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		get_filename(local_path + local_path_len);
		BOOL rv = DeleteFile(local_path);
		if (rv == 0)
		{
			printf("%s:%d \"%s\" last error is %lu %lx\n", __func__, __LINE__,
				local_path, GetLastError(), GetLastError());
		}
	}
}
