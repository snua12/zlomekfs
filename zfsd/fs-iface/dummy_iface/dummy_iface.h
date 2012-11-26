/*! 
 *  \file fs-iface.h
 *  \brief Dummy interface between filesystem and OS.
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

#ifndef DUMMY_IFACE_H
#define DUMMY_IFACE_H

#include <config.h>
#include <fh.h>

/*! Export filesystem to OS */
static inline bool fs_start(void)
{
	return true;
}

/*! Disconnect filesystem from OS */
static inline void fs_unmount(void)
{
}

/*! Cleanup filesystem interface internal structures */
static inline void fs_cleanup(void)
{
}

/*! Invalidate file handle DENTRY in kernel dentry cache.  */
static inline int32_t fs_invalidate_fh(ATTRIBUTE_UNUSED zfs_fh * fh)
{
	return ZFS_COULD_NOT_CONNECT;
}

/*! Invalidate dentry DENTRY in kernel dentry cache.  */
static inline int32_t fs_invalidate_dentry(ATTRIBUTE_UNUSED internal_dentry dentry, ATTRIBUTE_UNUSED bool volume_root_p)
{
	return ZFS_COULD_NOT_CONNECT;
}

#endif

/*! \page fs-iface ZlomekFS filesystem interface
 *
 * In order to implement new zlomekFS filesystem interface, you should write these functions:
 * \see fs_start, fs_unmount, fs_cleanup,  fs_invalidate_fh,  fs_invalidate_dentry.
 */
