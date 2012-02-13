#include <gtest/gtest.h>
#include <string.h>
#include <wchar.h>
#include "dokan_tools.h"

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
}

int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

