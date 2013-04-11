/*! 
 *  \file dir_test.c
 *  \brief  Test for getdents uClibc implementation.
 *  \author Ales Snuparek
 *
 */

#include <stdio.h>
#include "linux_dirent.h"

static void printdir(LINUX_DIR * dir)
{
	struct dirent * de = NULL;
	struct dirent entry;
	de = &entry;
	int rv = 0;
	do
	{
		rv = getdents_readdir_r(dir, &entry, &de);
		if (rv != 0)
		{
			perror("getdents_readdir_r");
			break;
		}

		if (de != NULL)
		{
			printf("%ld:%s\n", 
				getdents_telldir(dir),
				de->d_name);
		}
	}
	while(de != NULL);
}

int main(int argc, char * argv[])
{
	if (argc != 2) return 1;

	LINUX_DIR * dir = getdents_opendir(argv[1]);
	if (dir == NULL)
	{
		perror("getdents_opendir");
		return 1;
	}


	printdir(dir);
	long int pos = getdents_telldir(dir);
	printf("getdents_telldir ==  %ld\n", pos);
	getdents_seekdir(dir, pos);
	getdents_seekdir(dir, 3);
	printdir(dir);

	getdents_closedir(dir);

	return 0;
}
