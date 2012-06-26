#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "dir_tests.h"


#define DIR_COUNT 5
#define DIR_DEEP 3

int main(int argc, char * argv[])
{
	if (argc == 1)
	{
		printf("Usage: %s [test_dir] ... [test_dir]\n", argv[0]);
	}

	int i;
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
		
		init_filename_generator();
		generate_directory_content(test_path, DIR_COUNT, DIR_DEEP);
		init_filename_generator();
		cleanup_directory_content(test_path, DIR_COUNT, DIR_DEEP);
	}

	return EXIT_SUCCESS;
}

