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
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"
#include "constant.h"
#include "zfs_prot.h"
#include "data-coding.h"
#include "config.h"
#include "thread.h"
#include "network.h"
#include "node.h"
#include "dir.h"
#include "file.h"
#include "volume.h"
#include "log.h"

/* Mapping file type -> file mode.  */
unsigned int ftype2mode[FT_LAST_AND_UNUSED]
  = {0, S_IFREG, S_IFDIR, S_IFLNK, S_IFBLK, S_IFCHR, S_IFSOCK, S_IFIFO};

/* Request ID for next call.  */
static volatile uint32_t request_id;

/* Mutex for accessing request_id.  */
static pthread_mutex_t request_id_mutex;

/* void zfs_proc_null (void) */

void
zfs_proc_null_server (void *args, thread *t)
{
  DC *dc = &t->dc;

  encode_status (dc, ZFS_OK);
}

/* zfs_fh zfs_proc_root (void); */

void
zfs_proc_root_server (void *args, thread *t)
{
  DC *dc = &t->dc;

  encode_status (dc, ZFS_OK);
  encode_zfs_fh (dc, &root_fh);
}

/* dir_op_res zfs_proc_volume_root (volume_root_args); */

void
zfs_proc_volume_root_server (volume_root_args *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;
  volume vol;
  internal_fh ifh;

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_lookup (args->vid);
  zfsd_mutex_unlock (&volume_mutex);
  if (!vol)
    {
      encode_status (dc, ENOENT);
    }
  else
    {
      r = update_volume_root (vol, &ifh);
      zfsd_mutex_unlock (&vol->mutex);
      encode_status (dc, r);
      if (r == ZFS_OK)
	{
	  encode_zfs_fh (dc, &ifh->local_fh);
	  encode_fattr (dc, &ifh->attr);
	  zfsd_mutex_unlock (&ifh->mutex);
	}
    }
}

/* fattr zfs_proc_getattr (zfs_fh); */

void
zfs_proc_getattr_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;
  fattr fa;

  r = zfs_getattr (&fa, args);
  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_fattr (dc, &fa);
}

/* fattr zfs_proc_setattr (sattr_args); */

void
zfs_proc_setattr_server (sattr_args *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;
  fattr fa;

  r = zfs_setattr (&fa, &args->file, &args->attr);
  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_fattr (dc, &fa);
}

/* dir_op_res zfs_proc_lookup (dir_op_args); */

void
zfs_proc_lookup_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->dc;
  dir_op_res res;
  int32_t r;

  r = zfs_lookup (&res, &args->dir, &args->name);
  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_dir_op_res (dc, &res);

  free (args->name.str);
}

/* zfs_cap zfs_proc_open_by_name (open_name_args); */

void
zfs_proc_open_by_name_server (open_name_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* zfs_cap zfs_proc_open_by_fh (open_fh_args); */

void
zfs_proc_open_by_fh_server (open_fh_args *args, thread *t)
{
  DC *dc = &t->dc;
  zfs_cap res;
  int32_t r;

  r = zfs_open_by_fh (&res, &args->file, args->flags);
  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_zfs_cap (dc, &res);
}

/* void zfs_proc_close (zfs_cap); */

void
zfs_proc_close_server (zfs_cap *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;

  r = zfs_close (args);
  encode_status (dc, r);
}

/* read_dir_res zfs_proc_readdir (read_dir_args); */

void
zfs_proc_readdir_server (read_dir_args *args, thread *t)
{
  DC *dc = &t->dc;

  zfs_readdir (dc, &args->dir, args->cookie, args->count);
}

/* dir_op_res zfs_proc_mkdir (open_name_args); */

void
zfs_proc_mkdir_server (open_name_args *args, thread *t)
{
  DC *dc = &t->dc;
  dir_op_res res;
  int32_t r;

  r = zfs_mkdir (&res, &args->where.dir, &args->where.name, &args->attr);
  encode_status (dc, r);
  if (r == ZFS_OK)
    encode_dir_op_res (dc, &res);

  free (args->where.name.str);
}

/* void zfs_proc_rmdir (dir_op_args); */

void
zfs_proc_rmdir_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;

  r = zfs_rmdir (&args->dir, &args->name);
  encode_status (dc, r);
  free (args->name.str);
}

/* void zfs_proc_rename (rename_args); */

void
zfs_proc_rename_server (rename_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* void zfs_proc_link (link_args); */

void
zfs_proc_link_server (link_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* void zfs_proc_unlink (dir_op_args); */

void
zfs_proc_unlink_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->dc;
  int32_t r;

  r = zfs_rmdir (&args->dir, &args->name);
  encode_status (dc, r);
  free (args->name.str);
}

/* read_res zfs_proc_read (read_args); */

void
zfs_proc_read_server (read_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* write_res zfs_proc_write (write_args); */

void
zfs_proc_write_server (write_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_link_res zfs_proc_readlink (zfs_fh); */

void
zfs_proc_readlink_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* void zfs_proc_symlink (symlink_args); */

void
zfs_proc_symlink_server (symlink_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* void zfs_proc_mknod (mknod_args); */

void
zfs_proc_mknod_server (mknod_args *args, thread *t)
{
  DC *dc = &t->dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* ? zfs_proc_auth_stage1 (auth_stage1_args); */

void
zfs_proc_auth_stage1_server (auth_stage1_args *args, thread *t)
{
  DC *dc = &t->dc;
  network_fd_data_t *fd_data = t->u.network.fd_data;
  node nod;

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&fd_data->mutex);
  nod = node_lookup_name (args->node.str);
  zfsd_mutex_unlock (&node_mutex);
  if (nod)
    {
      /* FIXME: do the key authorization */
      fd_data->sid = nod->id;
      fd_data->auth = AUTHENTICATION_IN_PROGRESS;
      encode_status (dc, ZFS_OK);
      zfsd_mutex_unlock (&nod->mutex);
    }

  if (!nod)
    {
      zfsd_mutex_unlock (&fd_data->mutex);
      sleep (1);	/* FIXME: create constant or configuration directive */
      zfsd_mutex_lock (&fd_data->mutex);
      if (fd_data->fd >= 0 && fd_data->generation == t->u.network.generation)
	close_network_fd (fd_data->fd);
    }
  zfsd_mutex_unlock (&fd_data->mutex);

  free (args->node.str);
}

/* ? zfs_proc_auth_stage2 (auth_stage2_args); */

void
zfs_proc_auth_stage2_server (auth_stage2_args *args, thread *t)
{
  DC *dc = &t->dc;
  network_fd_data_t *fd_data = t->u.network.fd_data;
  node nod;
  bool authenticated = false;

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&fd_data->mutex);
  nod = node_lookup (fd_data->sid);
  zfsd_mutex_unlock (&node_mutex);
  if (nod)
    {
      /* FIXME: verify the authentication data */
      authenticated = true;
      if (authenticated)
	{
	  fd_data->auth = AUTHENTICATION_FINISHED;
	  encode_status (dc, ZFS_OK);
	  node_update_fd (nod, fd_data->fd, fd_data->generation);
	}
      zfsd_mutex_unlock (&nod->mutex);
    }

  if (!authenticated)
    {
      zfsd_mutex_unlock (&fd_data->mutex);
      sleep (1);	/* FIXME: create constant or configuration directive */
      zfsd_mutex_lock (&fd_data->mutex);
      if (fd_data->fd >= 0 && fd_data->generation == t->u.network.generation)
	close_network_fd (fd_data->fd);
    }
  zfsd_mutex_unlock (&fd_data->mutex);
}

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
int								\
zfs_proc_##FUNCTION##_client_1 (thread *t, ARGS *args, int fd)		\
{									\
  uint32_t req_id;							\
									\
  zfsd_mutex_lock (&request_id_mutex);				\
  req_id = request_id++;						\
  zfsd_mutex_unlock (&request_id_mutex);				\
  message (2, stderr, "sending request: ID=%u fn=%u\n", req_id, NUMBER);\
  start_encoding (&t->dc_call);					\
  encode_direction (&t->dc_call, DIR_REQUEST);				\
  encode_request_id (&t->dc_call, req_id);				\
  encode_function (&t->dc_call, NUMBER);				\
  if (!encode_##ARGS (&t->dc_call, args))				\
    return ZFS_REQUEST_TOO_LONG;					\
  finish_encoding (&t->dc_call);					\
									\
  send_request (t, req_id, fd);						\
									\
  return t->retval;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
int									\
zfs_proc_##FUNCTION##_client (thread *t, ARGS *args, node nod)		\
{									\
  int fd;								\
									\
  CHECK_MUTEX_LOCKED (&nod->mutex);					\
									\
  fd = node_connect_and_authenticate (t, nod);				\
  zfsd_mutex_unlock (&nod->mutex);					\
  if (fd < 0)								\
    return t->retval;							\
									\
  return zfs_proc_##FUNCTION##_client_1 (t, args, fd);			\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

/* Initialize data structures needed by this module.  */

void
initialize_zfs_prot_c ()
{
  zfsd_mutex_init (&request_id_mutex);
}

/* Cleanup data structures needed by this module.  */

void
cleanup_zfs_prot_c ()
{
  zfsd_mutex_destroy (&request_id_mutex);
}
