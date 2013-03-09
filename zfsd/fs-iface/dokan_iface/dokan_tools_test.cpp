/* ! \file dokan_tools_test.cpp
 *   \brief Tests for Dokan interface support functions
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

#include <gtest/gtest.h>
#include <string.h>
#include <wchar.h>
#include <iostream>
#include <errno.h>
#include <libgen.h>
#include <windows.h>
#include <winbase.h>
#include "dokan_tools.h"

#if 0 //now unused in this test
static void generate_path(WCHAR * path, size_t len)
{
	int i;
	for (i = 0; i < (len - 2); ++i)
	{
		int alphabet_index = ((i) % 27);
		if (alphabet_index == 0)
		{
			path[i] = L'/';
		}
		else
		{
			path[i] = L'a' + alphabet_index - 1;
		}
	}

	path[len - 1] = 0;
}
#endif

/*! \brief test for \ref unix_to_alternative_filename */
TEST(dokan_tool_test, unix_to_alternative_filename)
{
	WCHAR win_name[MAX_PATH];
	dir_entry de;
	de.ino = 0xff;
	xmkstring (&de.name, "123456789.ext");
	unix_to_alternative_filename(&de, win_name);
	ASSERT_STREQ(win_name, L"12345~FF.ext");
	xfreestring(&de.name);

	xmkstring (&de.name, "123456789.loog");
	unix_to_alternative_filename(&de, win_name);
	ASSERT_STREQ(win_name, L"12345~FF.loo");
	xfreestring(&de.name);

	xmkstring (&de.name, "12345678.ext");
	unix_to_alternative_filename(&de, win_name);
	ASSERT_STREQ(win_name, L"12345678.ext");
	xfreestring(&de.name);

	de.ino = 0xdeadbeef;
	xmkstring (&de.name, "123456789.ext");
	unix_to_alternative_filename(&de, win_name);
	ASSERT_STREQ(win_name, L"DEADBEEF.ext");
	xfreestring(&de.name);
}

/*! \brief test for \ref unix_to_alternative_filename */
TEST(dokan_tool_test, windows_to_unix_path)
{
	int rv;
	char unix_path[MAX_PATH];
	char red_zone[] = "r3dz0n3";
	const WCHAR win_path[] = L"\\a\\b\\c\\d\\e";
	const size_t win_path_len =  sizeof(win_path) / sizeof(win_path[0]);
	const WCHAR win_path_cz[] = L"\\ěščřžýáíé";
	const size_t win_path_cz_len = sizeof(win_path_cz) /  sizeof(win_path_cz[0]);

	// append redzone
	strcpy(unix_path + win_path_len, red_zone);
	rv = windows_to_unix_path(win_path, unix_path, win_path_len);
	ASSERT_EQ(0, rv);
	// redzone check
	ASSERT_STREQ(red_zone, unix_path + win_path_len);
	ASSERT_STREQ("/a/b/c/d/e", unix_path);

	rv = windows_to_unix_path(L"\\\\a\\\\b\\\\c\\\\d\\\\e", unix_path, win_path_len);
	ASSERT_EQ(0, rv);
	// redzone check
	ASSERT_STREQ(red_zone, unix_path + win_path_len);
	ASSERT_STREQ("/a/b/c/d/e", unix_path);

	// append redzone
	strcpy(unix_path + win_path_len -1, red_zone);
	rv = windows_to_unix_path(win_path, unix_path, win_path_len);
	ASSERT_EQ(0, rv);
	// redzone check
	ASSERT_STREQ("", unix_path + win_path_len - 1);
	ASSERT_STREQ(red_zone + 1, unix_path + win_path_len);
	ASSERT_STREQ("/a/b/c/d/e", unix_path);

	// append redzone
	strcpy(unix_path + win_path_len - 1, red_zone);
	rv = windows_to_unix_path(win_path, unix_path, win_path_len - 1);
	// redzone check
	ASSERT_STREQ(red_zone, unix_path + win_path_len - 1);
	ASSERT_EQ(ENAMETOOLONG, rv);

	// cz path check
	rv = windows_to_unix_path(win_path_cz, unix_path, sizeof(unix_path));
	ASSERT_EQ(0, rv);
}

/*! \brief test for basename and dirname  */
TEST(dokan_tool_test, basename_dirname)
{
	char dir_path[] = "/aa/bb";
	char * file = basename(dir_path);
	char * dir = dirname(dir_path);

	ASSERT_STREQ("bb", file);
	ASSERT_STREQ("/aa", dir);
}

int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

