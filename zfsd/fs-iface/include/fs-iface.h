/*! \file \brief filesystem interface.  */

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

#ifndef FS_IFACE_H
#define FS_IFACE_H

#include <config.h>
#include <fh.h>

#ifdef ENABLE_FS_INTERFACE
#ifdef HAVE_FUSE
#include "../fuse_iface/fuse_iface.h"
#else
#ifdef HAVE_DOKAN
#include "../dokan_iface/dokan_iface.h"
#else
#error "no filesystem interface was selected"
#endif
#endif
#endif

/*! Export filesystem to OS */
extern bool fs_start(void);

/*! Disconnect filesystem from OS */
extern void fs_unmount(void);

/*! Cleanup filesystem interface internal structures */
extern void fs_cleanup(void);

/*! Invalidate file handle DENTRY in kernel dentry cache.  */
extern int32_t fs_invalidate_fh(zfs_fh * fh);

/*! Invalidate dentry DENTRY in kernel dentry cache.  */
extern int32_t fs_invalidate_dentry(internal_dentry dentry, bool volume_root_p);


#endif
