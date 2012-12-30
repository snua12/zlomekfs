/*! 
 *  \file zfs_dirent.h
 *  \brief  Wrapper for POSIX directory operation API
 *  \author Ales Snuparek
 *
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
