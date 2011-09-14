#include <stdio.h>
#include "fh.h"

#ifndef ZFSIO_H
#define ZFS_IO_H

#define ZFS_TMP_SHARED_CONFIG_TEMPLATE "/tmp/.zfs_shared_configXXXXXXX"

typedef struct zfs_file_def
{
	FILE * stream;
	char tmp_file[sizeof(ZFS_TMP_SHARED_CONFIG_TEMPLATE)];
}
zfs_file;


zfs_file * zfs_fopen(zfs_fh * fh);

int zfs_fclose(zfs_file * file);

FILE * zfs_fdget(zfs_file * file);

#endif

