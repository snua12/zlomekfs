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
#include <pthread.h>
#include <inttypes.h>
#include "zfs_prot.h"
#include "data-coding.h"
#include "thread.h"
#include "server.h"
#include "node.h"
#include "dir.h"

#if 0
/* FIXME: These are some temporary dummy functions to make linker happy.  */
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_server (ARGS_TYPE *args, DC *dc)			\
{									\
  /* return zfs_##FUNCTION## (args, dc); */				\
  return ZFS_OK;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
#endif

int
zfs_proc_null_server (void *args, DC *dc)
{
  return ZFS_OK;
}

int
zfs_proc_root_server (void *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_volume_root_server (volume_root_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_getattr_server (zfs_fh *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_setattr_server (sattr_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_lookup_server (dir_op_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_open_by_name_server (open_name_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_open_by_fd_server (zfs_fh *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_close_server (zfs_fh *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_readdir_server (read_dir_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_mkdir_server (open_name_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_rmdir_server (dir_op_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_rename_server (rename_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_link_server (link_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_unlink_server (dir_op_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_read_server (read_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_write_server (write_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_readlink_server (zfs_fh *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_symlink_server (symlink_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_mknod_server (mknod_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_auth_stage1_server (auth_stage1_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

int
zfs_proc_auth_stage2_server (auth_stage2_args *args, DC *dc)
{
  return ZFS_UNKNOWN_FUNCTION;
}

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_client (thread *t, ARGS_TYPE *args, node nod)	\
{									\
  server_thread_data *td = &t->u.server;				\
  static uint32_t request_id;						\
									\
  start_encoding (&td->dc);						\
  encode_direction (&td->dc, DIR_REQUEST);				\
  encode_request_id (&td->dc, request_id++);				\
  encode_function (&td->dc, NUMBER);					\
  if (!encode_##ARGS_TYPE (&td->dc, args))				\
    return ZFS_REQUEST_TOO_LONG;					\
  finish_encoding (&td->dc);						\
									\
  send_request (td, nod);						\
									\
  return td->retval;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
