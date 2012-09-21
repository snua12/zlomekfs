/*! \file POSIXapi_test/file_tests.c
 *  \brief File tests
 *  \author Ales Snuparek
 */

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
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "syscall_collector.h"

/*! \brief test patter which is written to test file */
#define TEST_PATTERN "TEST STRING 123456789\n"

/*! \brief writes pattern to file
 *  \param h opened file descriptor
 */
static void write_test_pattern(int h)
{

	int bytes_written;
	char text[] = TEST_PATTERN;
	collect(SYSCALL_OP_WRITE, SYSCALL_STATE_BEGIN);
	bytes_written = write(h, text, sizeof(text) - 1);
	collect(SYSCALL_OP_WRITE, SYSCALL_STATE_END);
	if (bytes_written != (sizeof(text) - 1))

	{
		printf("WriteFile has failed, last error is %u %x\n", errno, errno);
	}
}

/*! \brief creates the test file
 *  \param path name of the test file
 */
void create_test_file(char * path)
{
	collect(SYSCALL_OP_OPEN, SYSCALL_STATE_BEGIN);
	int h = creat(path, O_RDWR | O_CREAT | S_IRWXU);
	collect(SYSCALL_OP_OPEN, SYSCALL_STATE_END);
			
	if (h == -1)
	{
		printf("last error is %d\n", errno);
	}

	write_test_pattern(h);

	collect(SYSCALL_OP_CLOSE, SYSCALL_STATE_BEGIN);
	close(h);
	collect(SYSCALL_OP_CLOSE, SYSCALL_STATE_END);
}

/*! \brief creates count of test files in a directory
 *  \param path direcotry where are test files created
 *  \param count how much of test files is created
 */
void generate_file_content(char * path, int count)
{
	char local_path[PATH_MAX + 1];
	strncpy(local_path, path, PATH_MAX - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
{
		get_filename(local_path + local_path_len);
		create_test_file(local_path);
	}
}

/*! \brief remove count of test files from a directory
 *  \param path direcotry where are test files created
 *  \param count how much of test files is created
 */
void cleanup_file_content(char * path, int count)
{
	char local_path[PATH_MAX + 1];
	strncpy(local_path, path, PATH_MAX - 1);
	int local_path_len = strlen(local_path);

	int i;
	for (i = 0; i < count; ++i)
	{
		get_filename(local_path + local_path_len);
		collect(SYSCALL_OP_UNLINK, SYSCALL_STATE_BEGIN);
		int rv = unlink(local_path);
		collect(SYSCALL_OP_UNLINK, SYSCALL_STATE_END);
		if (rv == -1)
		{
			printf("%s:%d \"%s\" last error is %u %x %s\n", __func__, __LINE__,
				local_path, errno, errno, strerror(errno));
		}
	}
}

