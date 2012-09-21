/*! \file win32api_test/win32api_op_test.c
 *  \brief File tests for win32api (test some filesystem operations)
 *  \author Ales Snuparek
 *
 *
 * This test creates a "searching in depth" tree of directories that on
 * the leaf level include files. Then remove asresarovou structure.
 * For the following operations: open, read, write, close mkdir and
 * rmdir is measured by the mean duration of these operations.
 * This test uses win32api.
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
#include <Windows.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "dir_tests.h"

/**
 * \brief       main entry
 * \param       command line argument
 * \return      error code
 */
int main(int argc, char * argv[])
{
	if (argc == 1)
	{
		printf("Usage: %s [test_dir] ... [test_dir]\n", argv[0]);
	}

	int i;
	// for every directory
	for (i = 1; i < argc; ++i)
	{
		char test_path[MAX_PATH];
		strncpy(test_path, argv[i], MAX_PATH);
		size_t test_path_len = strlen(test_path);
		if (test_path[test_path_len - 1] != '\\')
		{
			test_path[test_path_len + 1] = 0;
			test_path[test_path_len] = '\\';
		}

		printf("test_path is \"%s\"\n", test_path);
		
		test_move_file(test_path);
		cleanup_move_file(test_path);

		init_filename_generator();
		test_file_op(test_path);
		init_filename_generator();
		cleanup_file_op(test_path);
	}

	return EXIT_SUCCESS;
}
