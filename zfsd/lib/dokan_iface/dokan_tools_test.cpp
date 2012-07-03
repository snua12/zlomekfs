/* ! \file \brief Tests for Dokan interface support functions */

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


#include <gtest/gtest.h>
#include <string.h>
#include <wchar.h>
#include "dokan_tools.h"

#include <windows.h>
#include <winbase.h>

TEST(dokan_tools_test, file_path_to_dir_and_file)
{
	char dir_path[MAX_PATH];
	char file_name[MAX_PATH];

	file_path_to_dir_and_file(L"\\\\dir\\file", dir_path, file_name);
	ASSERT_STREQ("/dir", dir_path);
	ASSERT_STREQ("file", file_name);

	file_path_to_dir_and_file(L"\\\\dir\\dir1\\file", dir_path, file_name);
	ASSERT_STREQ("/dir/dir1", dir_path);
	ASSERT_STREQ("file", file_name);

	file_path_to_dir_and_file(L"\\\\only_file", dir_path, file_name);
	ASSERT_STREQ("/", dir_path);
	ASSERT_STREQ("only_file", file_name);

	file_path_to_dir_and_file(L"\\\\desktop.ini", dir_path, NULL);
	ASSERT_STREQ("/desktop.ini", dir_path);

	file_path_to_dir_and_file(L"\\\\", dir_path, NULL);
	ASSERT_STREQ("/", dir_path);

	// test UTF16 to UTF8 conversion and back
	WCHAR test_dir_path[] = L"řčžžýáíéřžýáížřýýžýážáýýáííáýáíýˇQˇWĚŘŤŽˇUˇIˇOˇPˇAŠĎˇFˇGˇHˇJˇKĽˇYˇXČˇVˇBŇˇMˇ´Q´wéŕ´tźúíó´poáś´d´f´g´h´jkĺý´xć´v´b´bn´m´*-+_";
	file_path_to_dir_and_file(test_dir_path, dir_path, file_name);
	WCHAR win_dir_path[MAX_PATH];
	unix_to_windows_filename(file_name, win_dir_path, MAX_PATH);
	ASSERT_STREQ(win_dir_path, test_dir_path);
}

int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

