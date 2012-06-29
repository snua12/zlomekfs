#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "file_tests.h"
#include "filename_generator.h"
#include "dir_tests.h"
#include "syscall_collector.h"


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
		char test_path[PATH_MAX];
		strncpy(test_path, argv[i], PATH_MAX);
		size_t test_path_len = strlen(test_path);
		if (test_path[test_path_len - 1] != '/')
		{
			test_path[test_path_len + 1] = 0;
			test_path[test_path_len] = '/';
		}

		printf("test_path is \"%s\"\n", test_path);
		
		collector_init();
		init_filename_generator();
		generate_directory_content(test_path, DIR_COUNT, DIR_DEEP);
		init_filename_generator();
		cleanup_directory_content(test_path, DIR_COUNT, DIR_DEEP);
		collector_print();
	}

	return EXIT_SUCCESS;
}

