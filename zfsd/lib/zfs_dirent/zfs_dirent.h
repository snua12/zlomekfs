#ifndef ZFS_DIRENT_H
#define ZFS_DIRENT_H
#include <dirent.h>
DIR *zfs_fdopendir(int fd);
void zfs_seekdir(DIR *dirp, long loc);
int zfs_readdir_r(DIR * dirp, struct dirent * entry, struct dirent ** result);
long zfs_telldir(DIR *dirp);
DIR * zfs_opendir(const char *dirname);
int zfs_closedir(DIR *dirp);
#endif
