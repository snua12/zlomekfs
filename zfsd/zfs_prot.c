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
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "zfs_prot.h"
#include "data-coding.h"
#include "config.h"
#include "thread.h"
#include "server.h"
#include "node.h"
#include "dir.h"
#include "volume.h"
#include "log.h"

/* Request ID for next call.  */
static volatile uint32_t request_id;

/* Mutex for accessing request_id.  */
static pthread_mutex_t request_id_mutex;

/* void zfs_proc_null (void) */

void
zfs_proc_null_server (void *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  encode_status (dc, ZFS_OK);
}

/* zfs_fh zfs_proc_root (void); */

void
zfs_proc_root_server (void *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  encode_status (dc, ZFS_OK);
  encode_zfs_fh (dc, &root_fh);
}

/* zfs_fh zfs_proc_volume_root (volume_root_args); */

void
zfs_proc_volume_root_server (volume_root_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

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
zfs_proc_getattr_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* attr_res zfs_proc_setattr (sattr_args); */

void
zfs_proc_setattr_server (sattr_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* dir_op_res zfs_proc_lookup (dir_op_args); */

void
zfs_proc_lookup_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* open_res zfs_proc_open_by_name (open_name_args); */

void
zfs_proc_open_by_name_server (open_name_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* open_res zfs_proc_open_by_fd (zfs_fh); */

void
zfs_proc_open_by_fd_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_close (zfs_fh); */

void
zfs_proc_close_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_dir_res zfs_proc_readdir (read_dir_args); */

void
zfs_proc_readdir_server (read_dir_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* dir_op_res zfs_proc_mkdir (open_name_args); */

void
zfs_proc_mkdir_server (open_name_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_rmdir (dir_op_args); */

void
zfs_proc_rmdir_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_rename (rename_args); */

void
zfs_proc_rename_server (rename_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_link (link_args); */

void
zfs_proc_link_server (link_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_unlink (dir_op_args); */

void
zfs_proc_unlink_server (dir_op_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_res zfs_proc_read (read_args); */

void
zfs_proc_read_server (read_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* write_res zfs_proc_write (write_args); */

void
zfs_proc_write_server (write_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* read_link_res zfs_proc_readlink (zfs_fh); */

void
zfs_proc_readlink_server (zfs_fh *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_symlink (symlink_args); */

void
zfs_proc_symlink_server (symlink_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* int zfs_proc_mknod (mknod_args); */

void
zfs_proc_mknod_server (mknod_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;

  /* TODO: write the function */
  encode_status (dc, ZFS_UNKNOWN_FUNCTION);
}

/* ? zfs_proc_auth_stage1 (auth_stage1_args); */

void
zfs_proc_auth_stage1_server (auth_stage1_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;
  server_fd_data_t *fd_data = t->u.server.fd_data;

  /* TODO: write the function */

  pthread_mutex_lock (&fd_data->mutex);
  fd_data->auth = AUTHENTICATION_IN_PROGRESS;
  pthread_mutex_unlock (&fd_data->mutex);
  encode_status (dc, ZFS_OK);
}

/* ? zfs_proc_auth_stage2 (auth_stage2_args); */

void
zfs_proc_auth_stage2_server (auth_stage2_args *args, thread *t)
{
  DC *dc = &t->u.server.dc;
  server_fd_data_t *fd_data = t->u.server.fd_data;

  /* TODO: write the function */

  pthread_mutex_lock (&fd_data->mutex);
  fd_data->auth = AUTHENTICATION_FINISHED;
  pthread_mutex_unlock (&fd_data->mutex);
  encode_status (dc, ZFS_OK);
}

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
static int								\
zfs_proc_##FUNCTION##_client_1 (thread *t, ARGS *args, int fd)		\
{									\
  server_thread_data *td = &t->u.server;				\
  uint32_t req_id;							\
									\
  pthread_mutex_lock (&request_id_mutex);				\
  req_id = request_id++;						\
  pthread_mutex_unlock (&request_id_mutex);				\
  message (2, stderr, "sending request: ID=%u fn=%u\n", req_id, NUMBER);\
  start_encoding (&td->dc_call);					\
  encode_direction (&td->dc_call, DIR_REQUEST);				\
  encode_request_id (&td->dc_call, req_id);				\
  encode_function (&td->dc_call, NUMBER);				\
  if (!encode_##ARGS (&td->dc_call, args))				\
    return ZFS_REQUEST_TOO_LONG;					\
  finish_encoding (&td->dc_call);					\
									\
  send_request (t, req_id, fd);						\
									\
  return td->retval;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

/* If node NOD is connected return true and lock SERVER_FD_DATA[NOD->FD].MUTEX.
   This function expects NOD->MUTEX to be locked.  */

static bool
node_connected_p (node nod)
{
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&nod->mutex) == 0)
    abort ();
#endif

  if (nod->fd < 0)
    return false;

  pthread_mutex_lock (&server_fd_data[nod->fd].mutex);
  if (nod->generation != server_fd_data[nod->fd].generation)
    {
      pthread_mutex_unlock (&server_fd_data[nod->fd].mutex);
      return false;
    }

  return true;
}

/* Connect to node NOD, return open file descriptor.  */

int
node_connect (node nod)
{
  struct addrinfo *addr, *a;
  int s;
  int err;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&nod->mutex) == 0)
    abort ();
#endif

  /* Lookup the IP address.  */
  if ((err = getaddrinfo (nod->name, NULL, NULL, &addr)) != 0)
    {
      message (-1, stderr, "getaddrinfo(): %s\n", gai_strerror (err));
      return -1;
    }

  for (a = addr; a; a = a->ai_next)
    {
      switch (a->ai_family)
	{
	  case AF_INET:
	    if (a->ai_socktype == SOCK_STREAM
		&& a->ai_protocol == IPPROTO_TCP)
	      {
		s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		/* Connect the server socket to ZFS_PORT.  */
		((struct sockaddr_in *)a->ai_addr)->sin_port = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) >= 0)
		  goto node_connected;
	      }
	    break;

	  case AF_INET6:
	    if (a->ai_socktype == SOCK_STREAM
		&& a->ai_protocol == IPPROTO_TCP)
	      {
		s = socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0)
		  {
		    message (-1, stderr, "socket(): %s\n", strerror (errno));
		    break;
		  }

		/* Connect the server socket to ZFS_PORT.  */
		((struct sockaddr_in6 *)a->ai_addr)->sin6_port
		  = htons (ZFS_PORT);
		if (connect (s, a->ai_addr, a->ai_addrlen) >= 0)
		  goto node_connected;
	      }
	    break;
	}
    }

  message (-1, stderr, "Could not connect to %s\n", nod->name);
  close (s);
  freeaddrinfo (addr);
  return -1;

node_connected: 
  freeaddrinfo (addr);
  nod->fd = s;
  nod->auth = AUTHENTICATION_NONE;
  nod->conn = CONNECTION_FAST; /* FIXME */
  message (2, stderr, "FD %d connected to %s\n", s, nod->name);
  return s;
}

/* Authenticate connection with node NOD using data of thread T.
   On success leave SERVER_FD_DATA[NOD->FD].MUTEX lcoked.  */

static bool
node_authenticate (thread *t, node nod)
{
  auth_stage1_args args1;
  auth_stage2_args args2;

  memset (&args1, 0, sizeof (args1));
  memset (&args2, 0, sizeof (args2));
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&nod->mutex) == 0)
    abort ();
#endif

  /* FIXME: really do authentication; currently the functions are empty.  */
#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[nod->fd].mutex) == 0)
    abort ();
#endif
  args1.node.len = node_name_len;
  args1.node.str = node_name;
  if (zfs_proc_auth_stage1_client_1 (t, &args1, nod->fd) != ZFS_OK)
    goto node_authenticate_error;
  if (!node_connected_p (nod))
    goto node_authenticate_error;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[nod->fd].mutex) == 0)
    abort ();
#endif
  nod->auth = AUTHENTICATION_IN_PROGRESS;
  server_fd_data[nod->fd].auth = AUTHENTICATION_IN_PROGRESS;

  if (zfs_proc_auth_stage2_client_1 (t, &args2, nod->fd) != ZFS_OK)
    goto node_authenticate_error;
  if (!node_connected_p (nod))
    goto node_authenticate_error;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_fd_data[nod->fd].mutex) == 0)
    abort ();
#endif
  nod->auth = AUTHENTICATION_FINISHED;
  server_fd_data[nod->fd].auth = AUTHENTICATION_FINISHED;
  return true;

node_authenticate_error:
  message (2, stderr, "not auth\n");
  pthread_mutex_lock (&server_fd_data[nod->fd].mutex);
  close_server_fd (nod->fd);
  pthread_mutex_unlock (&server_fd_data[nod->fd].mutex);
  nod->auth = AUTHENTICATION_NONE;
  nod->conn = CONNECTION_NONE;
  nod->fd = -1;
  return false;
}

/* Check whether node NOD is connected and authenticated. If not do so.
   Return open file descriptor and leave its SERVER_FD_DATA locked.  */

static int
zfs_proc_client_connect (thread *t, node nod)
{
  server_thread_data *td = &t->u.server;
  int fd;

  pthread_mutex_lock (&nod->mutex);
  if (!node_connected_p (nod))
    {

      fd = node_connect (nod);
      if (fd < 0)
	{
	  td->retval = ZFS_COULD_NOT_CONNECT;
	  pthread_mutex_unlock (&nod->mutex);
	  return -1;
	}
      add_fd_to_active (fd, nod);

      if (!node_authenticate (t, nod))
	{
	  td->retval = ZFS_COULD_NOT_AUTH;
	  pthread_mutex_unlock (&nod->mutex);
	  return -1;
	}
    }
  else
    fd = nod->fd;

  pthread_mutex_unlock (&nod->mutex);
  return fd;
}

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH)		\
int									\
zfs_proc_##FUNCTION##_client (thread *t, ARGS *args, node nod)		\
{									\
  server_thread_data *td = &t->u.server;				\
  int fd;								\
									\
  fd = zfs_proc_client_connect (t, nod);				\
  if (fd < 0)								\
    return td->retval;							\
									\
  return zfs_proc_##FUNCTION##_client_1 (t, args, fd);			\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

/* Initialize data structures needed by this module.  */

void
initialize_zfs_prot_c ()
{
  pthread_mutex_init (&request_id_mutex, NULL);
}

/* Cleanup data structures needed by this module.  */

void
cleanup_zfs_prot_c ()
{
  pthread_mutex_destroy (&request_id_mutex);
}
