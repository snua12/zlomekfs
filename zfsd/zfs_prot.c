/* ZFS protocol.
   Copyright (C) 2003 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include "zfs_prot.h"

/* FIXME: These are some temporary dummy functions to make linker happy.  */
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_server (ARGS_TYPE *args, DC *dc)			\
{									\
  return ZFS_OK;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

#if 0
zfs_proc_null
zfs_proc_root
zfs_proc_volume_root
zfs_proc_getattr
zfs_proc_setattr
zfs_proc_lookup
zfs_proc_open_by_name
zfs_proc_open_by_fd
zfs_proc_close
zfs_proc_readdir
zfs_proc_mkdir
zfs_proc_rmdir
zfs_proc_rename
zfs_proc_link
zfs_proc_unlink
zfs_proc_read
zfs_proc_write
zfs_proc_readlink
zfs_proc_symlink
zfs_proc_mknod
#endif
