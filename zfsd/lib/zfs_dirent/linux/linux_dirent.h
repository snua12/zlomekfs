/*! 
 *  \file linux_dirent.h
 *  \brief  Wrapper for POSIX directory operation API for Android platform
 *  \author Ales Snuparek based on Mike Shal <marfey@gmail.com>
 *
 */

#include <dirent.h>

#ifndef LINUX_DIRENT_H
#define LINUX_DIRENT_H

typedef struct __dirstream LINUX_DIR;

LINUX_DIR *getdents_opendir(const char *name);

LINUX_DIR *getdents_fdopendir(int fd);

int getdents_closedir(LINUX_DIR * dir);

struct dirent *getdents_readdir(LINUX_DIR * dir);

int getdents_readdir_r(LINUX_DIR *dir, struct dirent *entry, struct dirent **result);

void getdents_seekdir(LINUX_DIR * dir, long int offset);

long int getdents_telldir(LINUX_DIR * dir);

#endif
