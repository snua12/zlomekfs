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
#include <errno.h>
#include "zfs_prot.h"
#include "data-coding.h"
#include "thread.h"
#include "server.h"
#include "node.h"
#include "dir.h"
#include "volume.h"

/* void zfs_proc_null (void) */

void
zfs_proc_null_server (void *args, DC *dc)
{
  encode_status (dc, ZFS_OK);
}

/* zfs_fh zfs_proc_root (void); */

void
zfs_proc_root_server (void *args, DC *dc)
{
  encode_status (dc, ZFS_OK);
  encode_zfs_fh (dc, &root_fh);
}

/* zfs_fh zfs_proc_volume_root (volume_root_args); */

void
zfs_proc_volume_root_server (volume_root_args *args, DC *dc)
{
  int32_t r;
  volume vol;
  zfs_fh fh;

  vol = volume_lookup (args->vid);
  if (!vol)
    r = ENOENT;
  else
    r = get_volume_root (vol, &fh, NULL);

  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_zfs_fh (dc, &fh);
}

/* attr_res zfs_proc_getattr (zfs_fh); */

void
zfs_proc_getattr_server (zfs_fh *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* attr_res zfs_proc_setattr (sattr_args); */

void
zfs_proc_setattr_server (sattr_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* dir_op_res zfs_proc_lookup (dir_op_args); */

void
zfs_proc_lookup_server (dir_op_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* open_res zfs_proc_open_by_name (open_name_args); */

void
zfs_proc_open_by_name_server (open_name_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* open_res zfs_proc_open_by_fd (zfs_fh); */

void
zfs_proc_open_by_fd_server (zfs_fh *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_close (zfs_fh); */

void
zfs_proc_close_server (zfs_fh *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_dir_res zfs_proc_readdir (read_dir_args); */

void
zfs_proc_readdir_server (read_dir_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* dir_op_res zfs_proc_mkdir (open_name_args); */

void
zfs_proc_mkdir_server (open_name_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_rmdir (dir_op_args); */

void
zfs_proc_rmdir_server (dir_op_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_rename (rename_args); */

void
zfs_proc_rename_server (rename_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_link (link_args); */

void
zfs_proc_link_server (link_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_unlink (dir_op_args); */

void
zfs_proc_unlink_server (dir_op_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_res zfs_proc_read (read_args); */

void
zfs_proc_read_server (read_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* write_res zfs_proc_write (write_args); */

void
zfs_proc_write_server (write_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_link_res zfs_proc_readlink (zfs_fh); */

void
zfs_proc_readlink_server (zfs_fh *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_symlink (symlink_args); */

void
zfs_proc_symlink_server (symlink_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_mknod (mknod_args); */

void
zfs_proc_mknod_server (mknod_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* ? zfs_proc_auth_stage1 (auth_stage1_args); */

void
zfs_proc_auth_stage1_server (auth_stage1_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_OK);
}

/* ? zfs_proc_auth_stage2 (auth_stage2_args); */

void
zfs_proc_auth_stage2_server (auth_stage2_args *args, DC *dc)
{
  /* TODO: write the function */
  encode_status (dc, ZFS_OK);
}

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_client (thread *t, ARGS_TYPE *args, node nod)	\
{									\
  server_thread_data *td = &t->u.server;				\
  static uint32_t request_id;						\
									\
  start_encoding (&td->dc_call);					\
  encode_direction (&td->dc_call, DIR_REQUEST);				\
  encode_request_id (&td->dc_call, request_id++);			\
  encode_function (&td->dc_call, NUMBER);				\
  if (!encode_##ARGS_TYPE (&td->dc_call, args))				\
    return ZFS_REQUEST_TOO_LONG;					\
  finish_encoding (&td->dc_call);					\
									\
  send_request (td, nod);						\
									\
  return td->retval;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC
