#include <stdio.h>
#include "fh.h"

#ifndef ZFSIO_H
#define ZFS_IO_H

typedef struct zfs_file_def zfs_file;

zfs_file * zfs_fopen(zfs_fh * fh);

int zfs_fclose(zfs_file * file);

FILE * zfs_fdget(zfs_file * file);

#endif

