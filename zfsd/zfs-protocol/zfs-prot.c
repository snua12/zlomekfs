/*! \file zfs-prot.c \brief ZFS protocol.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Martin Zlomek, Rastislav
   Wartiak

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

# include "system.h"
# include <inttypes.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <errno.h>
# include "zfs-prot.h"
#include "pthread-wrapper.h"
# include "data-coding.h"
# include "reread_config.h"
# include "thread.h"
# include "network.h"
# include "node.h"
# include "dir.h"
# include "file.h"
# include "volume.h"
# include "log.h"
# include "user-group.h"

/*! Mapping file type -> file mode.  */
unsigned int ftype2mode[FT_LAST_AND_UNUSED]
	= { 0, S_IFREG, S_IFDIR, S_IFLNK, S_IFBLK, S_IFCHR, S_IFSOCK, S_IFIFO };

/*! Convert ZFS error to system error */
int zfs_error(int error)
{
	if (error > 0)
		return -error;

	switch (error)
	{
	case ZFS_OK:
		return 0;
	case ZFS_REQUEST_TOO_LONG:
	case ZFS_INVALID_REQUEST:
	case ZFS_REPLY_TOO_LONG:
	case ZFS_INVALID_REPLY:
		return -EPROTO;
	case ZFS_UNKNOWN_FUNCTION:
		return -EOPNOTSUPP;
	case ZFS_COULD_NOT_CONNECT:
	case ZFS_COULD_NOT_AUTH:
		return -ENOTCONN;
	case ZFS_STALE:
	case ZFS_METADATA_ERROR:
	case ZFS_UPDATE_FAILED:
	case ZFS_EXITING:
	case ZFS_CONNECTION_CLOSED:
	case ZFS_REQUEST_TIMEOUT:
	default:
		return -ESTALE;
	}
}

/*! returns id for request */
static uint32_t zfs_get_next_request_id(void)
{
	static pthread_mutex_t reqiest_id_mutex = ZFS_MUTEX_INITIALIZER;
	/*! Request ID for next call.  */
	static volatile uint32_t request_id = 0;

	zfsd_mutex_lock(&reqiest_id_mutex);
	uint32_t rv = request_id;
	request_id ++;
	zfsd_mutex_unlock(&reqiest_id_mutex);

	return rv;
}

/*! void zfs_proc_null (void)

Do nothing. Just test whether the requests can be sent.  */
void
zfs_proc_null_server (ATTRIBUTE_UNUSED void *args, DC *dc,
		  ATTRIBUTE_UNUSED void *data)
{
      encode_status (dc, ZFS_OK);
}

/*! data_buffer zfs_proc_ping (data_buffer);

   Ping. The receiver sends back what the sender sent.  */
void
zfs_proc_ping_server(data_buffer * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	encode_status(dc, ZFS_OK);
	encode_data_buffer(dc, args);
}

/*! zfs_fh zfs_proc_root (void);

   Get file handle of root.  */
void
zfs_proc_root_server(ATTRIBUTE_UNUSED void *args, DC * dc,
					 ATTRIBUTE_UNUSED void *data)
{
	encode_status(dc, ZFS_OK);
	encode_zfs_fh(dc, &root_fh);
}

/*! zfs_fh, fattr zfs_proc_volume_root (uint32_t vid);

   Get the file handle and attributes of the volume root.  */
void
zfs_proc_volume_root_server(volume_root_args * args, DC * dc,
							ATTRIBUTE_UNUSED void *data)
{
	dir_op_res res;
	int32_t r;

	r = zfs_volume_root(&res, args->vid);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_dir_op_res(dc, &res);
}

/*! fattr zfs_proc_getattr (zfs_fh);

   Get attributes of the file.  */
void
zfs_proc_getattr_server(zfs_fh * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	int32_t r;
	fattr fa;

	r = zfs_getattr(&fa, args);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_fattr(dc, &fa);
}

/*! fattr zfs_proc_setattr (zfs_fh file, sattr attr);

   Set attributes of the file.  */
void
zfs_proc_setattr_server(setattr_args * args, DC * dc,
						ATTRIBUTE_UNUSED void *data)
{
	int32_t r;
	fattr fa;

	r = zfs_setattr(&fa, &args->file, &args->attr, false);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_fattr(dc, &fa);
}

/*! zfs_fh, fattr zfs_proc_lookup (zfs_fh dir, string filename);

   Lookup the file name.  */
void
zfs_proc_lookup_server(dir_op_args * args, DC * dc,
					   ATTRIBUTE_UNUSED void *data)
{
	dir_op_res res;
	int32_t r;

	r = zfs_lookup(&res, &args->dir, &args->name);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_dir_op_res(dc, &res);
}

/*! zfs_cap, zfs_fh, fattr zfs_proc_create (zfs_fh dir, string filename,
   uint32_t flags, sattr attr);

   Create the file.  \p flags are O_*, as defined in <fcntl.h>.  */
void
zfs_proc_create_server(create_args * args, DC * dc,
					   ATTRIBUTE_UNUSED void *data)
{
	create_res res;
	int32_t r;

	r = zfs_create(&res, &args->where.dir, &args->where.name, args->flags,
				   &args->attr);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_create_res(dc, &res);
}

/*! zfs_cap zfs_proc_open (zfs_fh file, uint32_t flags);

   Open the file or directory.  \p flags are O_*, as defined in <fcntl.h>. */
void
zfs_proc_open_server(open_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	zfs_cap res;
	int32_t r;

	r = zfs_open(&res, &args->file, args->flags);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_zfs_cap(dc, &res);
}

/*! void zfs_proc_close (zfs_cap);

   Close the file or directory.  */
void
zfs_proc_close_server(zfs_cap * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_close(args);
	encode_status(dc, r);
}

/*! uint32_t count, int8_t eof, dir_entry... zfs_proc_readdir (zfs_cap cap,
   int32_t cookie, uint32_t size);

   Read the contents of the directory, returning at most \p size bytes of
   dentries, starting at the position specified by \p cookie. \p cookie can be 
   0 to start at the beginning, or a value returned by a prevous
   zfs_proc_readdir () operation.

   Return a sequence of of \p count directory entries; \p eof is non-zero if
   the end of the directory was reached.

   Each directory entry has the following structure: - <tt>uint32_t ino</tt> - 
   <tt>int32_t cookie</tt>: can be used as a parameter in the following
   zfs_proc_readdir() operation to continue reading after this directory entry
   - <tt>string filename</tt> */
void
zfs_proc_readdir_server(read_dir_args * args, DC * dc,
						ATTRIBUTE_UNUSED void *data)
{
	int32_t r;
	char *old_pos, *cur_pos;
	unsigned int old_len, cur_len;
	dir_list list;

	list.n = 0;
	list.eof = 0;
	list.buffer = dc;

	old_pos = dc->cur_pos;
	old_len = dc->cur_length;
	encode_status(dc, ZFS_OK);
	encode_dir_list(dc, &list);

	r = zfs_readdir(&list, &args->cap, args->cookie, args->count,
					&filldir_encode);

	cur_pos = dc->cur_pos;
	cur_len = dc->cur_length;
	dc->cur_pos = old_pos;
	dc->cur_length = old_len;

	encode_status(dc, r);
	if (r == ZFS_OK)
	{
		encode_dir_list(dc, &list);
		dc->cur_pos = cur_pos;
		dc->cur_length = cur_len;
	}
}

/*! zfs_fh, fattr zfs_proc_mkdir (zfs_fh dir, string filename, sattr attr);

   Create the directory.  */
void
zfs_proc_mkdir_server(mkdir_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	dir_op_res res;
	int32_t r;

	r = zfs_mkdir(&res, &args->where.dir, &args->where.name, &args->attr);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_dir_op_res(dc, &res);
}

/*! void zfs_proc_rmdir (zfs_fh dir, string filename);

   Delete the directory.  */
void
zfs_proc_rmdir_server(dir_op_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_rmdir(&args->dir, &args->name);
	encode_status(dc, r);
}

/*! void zfs_proc_rename (zfs_fh from_dir, string from_filename, zfs_fh
   to_dir, string to_filename);

   Rename the file or directory.  */
void
zfs_proc_rename_server(rename_args * args, DC * dc,
					   ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_rename(&args->from.dir, &args->from.name,
				   &args->to.dir, &args->to.name);
	encode_status(dc, r);
}

/*! void zfs_proc_link (zfs_fh from, zfs_fh to_dir, string to_filename);

   Link the file.  */
void
zfs_proc_link_server(link_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_link(&args->from, &args->to.dir, &args->to.name);
	encode_status(dc, r);
}

/*! void zfs_proc_unlink (zfs_fh dir, string filename);

   Delete the file.  */
void
zfs_proc_unlink_server(dir_op_args * args, DC * dc,
					   ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_unlink(&args->dir, &args->name);
	encode_status(dc, r);
}

/*! data_buffer, uint64_t version zfs_proc_read (zfs_cap cap, uint64_t
   offset, uint32_t count);

   Read from the file.  Returned \p version is the file version from which the
   data was read. */
void
zfs_proc_read_server(read_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	read_res res;
	int32_t r;
	char *old_pos;
	unsigned int old_len;

	old_pos = dc->cur_pos;
	old_len = dc->cur_length;
	encode_status(dc, ZFS_OK);
	encode_uint32_t(dc, 0);
	res.data.buf = dc->cur_pos;
	dc->cur_pos = old_pos;
	dc->cur_length = old_len;

	r = zfs_read(&res, &args->cap, args->offset, args->count, true);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_read_res(dc, &res);
}

/*! uint32_t written, uint64_t version zfs_proc_write (zfs_cap cap, uint64_t
   offset, data_buffer data);

   Write to the file.  Returned \p version is the file version after writing
   the data. */
void
zfs_proc_write_server(write_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	write_res res;
	int32_t r;

	args->remote = true;
	r = zfs_write(&res, args);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_write_res(dc, &res);
}

/*! string zfs_proc_readlink (zfs_fh);

   Read the symlink.  */
void
zfs_proc_readlink_server(zfs_fh * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	int32_t r;
	read_link_res res;

	r = zfs_readlink(&res, args);
	encode_status(dc, r);
	if (r == ZFS_OK)
	{
		encode_read_link_res(dc, &res);
		free(res.path.str);
	}
}

/*! zfs_fh, fattr zfs_proc_symlink (zfs_fh from_dir, string from_filename,
   string to, sattr attr);

   Create the symlink.  */
void
zfs_proc_symlink_server(symlink_args * args, DC * dc,
						ATTRIBUTE_UNUSED void *data)
{
	dir_op_res res;
	int32_t r;

	r = zfs_symlink(&res, &args->from.dir, &args->from.name, &args->to,
					&args->attr);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_dir_op_res(dc, &res);
}

/*! zfs_fh, fattr zfs_proc_mknod (zfs_fh dir, string filename, sattr attr,
   ftype type, uint32_t rdev);

   Create the special file.  */
void
zfs_proc_mknod_server(mknod_args * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	dir_op_res res;
	int32_t r;

	r = zfs_mknod(&res, &args->where.dir, &args->where.name, &args->attr,
				  args->type, args->rdev);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_dir_op_res(dc, &res);
}

/*! string nodename zfs_proc_auth_stage1 (string nodename);

   1st stage of authentication.  */
void zfs_proc_auth_stage1_server(auth_stage1_args * args, DC * dc, void *data)
{
	auth_stage1_res res;
	network_thread_data *t_data = (network_thread_data *) data;
	fd_data_t *fd_data = t_data->fd_data;
	node nod;

	nod = node_lookup_name(&args->node);
	zfsd_mutex_lock(&fd_data->mutex);
	if (nod)
	{
		/* FIXME: do the key authorization */

		message(LOG_INFO, FACILITY_NET, "FD %d connected to %s (%s)\n",
				fd_data->fd, nod->name.str, nod->host_name.str);
		fd_data->sid = nod->id;
		fd_data->auth = AUTHENTICATION_STAGE_1;
		zfsd_cond_broadcast(&fd_data->cond);
		update_node_fd(nod, fd_data->fd, fd_data->generation, false);
		zfsd_mutex_unlock(&nod->mutex);

		encode_status(dc, ZFS_OK);
		xstringdup(&res.node, &this_node->name);
		encode_auth_stage1_res(dc, &res);
		free(res.node.str);
	}

	if (!nod)
	{
		zfsd_mutex_unlock(&fd_data->mutex);
		sleep(1);				/* FIXME: create constant or configuration
								   directive */
		zfsd_mutex_lock(&fd_data->mutex);
		if (fd_data->fd >= 0 && fd_data->generation == t_data->generation)
			close_network_fd(fd_data->fd);
	}
	zfsd_mutex_unlock(&fd_data->mutex);
}

/*! void zfs_proc_auth_stage2 (uint8_t connection_speed);

   2nd stage of authentication.  */
void zfs_proc_auth_stage2_server(auth_stage2_args * args, DC * dc, void *data)
{
	network_thread_data *t_data = (network_thread_data *) data;
	fd_data_t *fd_data = t_data->fd_data;
	node nod;
	bool authenticated = false;

	nod = node_lookup(fd_data->sid);
	zfsd_mutex_lock(&fd_data->mutex);
	if (nod)
	{
		/* FIXME: verify the authentication data */
		authenticated = true;
		if (authenticated)
		{
			fd_data->auth = AUTHENTICATION_FINISHED;
			fd_data->conn = CONNECTION_ESTABLISHED;
			fd_data->speed = args->speed;
			zfsd_cond_broadcast(&fd_data->cond);
			encode_status(dc, ZFS_OK);
		}
		zfsd_mutex_unlock(&nod->mutex);
	}

	if (!authenticated)
	{
		zfsd_mutex_unlock(&fd_data->mutex);
		sleep(1);				/* FIXME: create constant or configuration
								   directive */
		zfsd_mutex_lock(&fd_data->mutex);
		if (fd_data->fd >= 0 && fd_data->generation == t_data->generation)
			close_network_fd(fd_data->fd);
	}
	zfsd_mutex_unlock(&fd_data->mutex);
}

/*! uint32_t count, uint32_t __unused, uint64_t size, uint64_t version,
   uint64_t offset[count], uint32_t length[count], uint8_t
   md5sum[count][#MD5_SIZE] zfs_proc_md5sum (zfs_cap cap, uint32_t count,
   int8_t ignore_changes, uint64_t offset[count], uint32_t length[count]);

   Compute MD5 sum of file blocks. The operation fails with #ZFS_CHANGED if
   the file is changed while computing the hash value unless \p ignore_changes 
   is nonzero. */
void
zfs_proc_md5sum_server(md5sum_args * args, DC * dc,
					   ATTRIBUTE_UNUSED void *data)
{
	int32_t r;
	md5sum_res md5;

	r = local_md5sum(&md5, args);
	encode_status(dc, r);
	if (r == ZFS_OK)
		encode_md5sum_res(dc, &md5);
}

/*! string zfs_proc_file_info (zfs_fh);

   Get relative path for the file handle.  */
void
zfs_proc_file_info_server(zfs_fh * args, DC * dc, ATTRIBUTE_UNUSED void *data)
{
	file_info_res res;
	int32_t r;

	r = zfs_file_info(&res, args);
	encode_status(dc, r);
	if (r == ZFS_OK)
	{
		encode_zfs_path(dc, &res.path);
		free(res.path.str);
	}
}

/*! N/A zfs_proc_reread_config (string path);

   Reread the configuration file specified by relative \p path within the
   configuration volume.  An one-way request.  */
void
zfs_proc_reread_config_server(reread_config_args * args,
							  ATTRIBUTE_UNUSED DC * dc,
							  ATTRIBUTE_UNUSED void *data)
{
	string relative_path;
	thread *t;

	t = (thread *) pthread_getspecific(thread_data_key);
#ifdef ENABLE_CHECKING
	if (t == NULL)
		zfsd_abort();
#endif

	xstringdup(&relative_path, &args->path);
	add_reread_config_request(&relative_path, t->from_sid);
}

/*! void reintegrate (zfs_fh fh, int8_t status);

   If \p status != 0, lock \p fh for reintegration (exclude other
   reintegration attempts); if \p status == 0, release the reintegration lock
   on \p fh.

   Note that reintegrate_ver () releases the reintegration lock as well. */
void
zfs_proc_reintegrate_server(reintegrate_args * args, DC * dc,
							ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_reintegrate(&args->fh, args->status);
	encode_status(dc, r);
}

/*! void reintegrate_add (zfs_fh fh, zfs_fh dir, string filename);

   Add the file \p fh as \p filename in \p dir.  If \p fh is a directory, it
   is moved there, otherwise a link is added without removing the original
   file. */
void
zfs_proc_reintegrate_add_server(reintegrate_add_args * args, DC * dc,
								ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_reintegrate_add(&args->fh, &args->dir, &args->name);
	encode_status(dc, r);
}

/*! void reintegrate_del (zfs_fh fh, zfs_fh dir, string filename, int8_t
   destroy);

   Delete \p filename in \p dir, which is \p fh.  If \p destroy is non-zero,
   delete it irreversibly, otherwise move it to a shadow directory. */
void
zfs_proc_reintegrate_del_server(reintegrate_del_args * args, DC * dc,
								ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_reintegrate_del(&args->fh, &args->dir, &args->name,
							args->destroy_p);
	encode_status(dc, r);
}

/*! void reintegrate_ver (zfs_fh fh, uint64_t version_inc);

   Increase the version of \p fh by \p version_inc and release the
   reintegration lock on \p fh. */
void
zfs_proc_reintegrate_ver_server(reintegrate_ver_args * args, DC * dc,
								ATTRIBUTE_UNUSED void *data)
{
	int32_t r;

	r = zfs_reintegrate_ver(&args->fh, args->version_inc);
	encode_status(dc, r);
}

/*! Call remote FUNCTION with ARGS using data structures in thread T and
   return its error code.  Use FD for communication with remote node.  */
#define ZFS_CALL_CLIENT
#define ZFS_CALL_KERNEL
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
int32_t									\
zfs_proc_##FUNCTION##_client_1 (thread *t, ARGS *args, int fd)		\
{									\
  uint32_t req_id;							\
                                                                        \
  CHECK_MUTEX_LOCKED (&fd_data_a[fd].mutex);				\
                                                                        \
  req_id = zfs_get_next_request_id();					\
  message (LOG_INFO, FACILITY_NET, "sending request: ID=%u fn=%u (%s)\n", req_id, NUMBER, #NAME);\
  start_encoding (t->dc_call);						\
  encode_direction (t->dc_call, CALL_MODE);				\
  encode_request_id (t->dc_call, req_id);				\
  encode_function (t->dc_call, NUMBER);					\
  if (!encode_##ARGS (t->dc_call, args))				\
    {									\
      zfsd_mutex_unlock (&fd_data_a[fd].mutex);				\
      return ZFS_REQUEST_TOO_LONG;					\
    }									\
  finish_encoding (t->dc_call);						\
                                                                        \
  if (CALL_MODE == DIR_ONEWAY)						\
    send_oneway_request (t, fd);					\
  else									\
    send_request (t, req_id, fd);					\
                                                                        \
  return t->retval;							\
}
#include "zfs-prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_KERNEL
#undef ZFS_CALL_CLIENT

/*! Call remote FUNCTION with ARGS on node NOD using data structures in
   thread T and return its error code, store file descriptor connected to NOD
   to FD.  */
#define ZFS_CALL_CLIENT
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
int32_t									\
zfs_proc_##FUNCTION##_client (thread *t, ARGS *args, node nod, int *fd)	\
{									\
  CHECK_MUTEX_LOCKED (&nod->mutex);					\
                                                                        \
  *fd = node_connect_and_authenticate (t, nod, AUTH);			\
  if (*fd < 0)								\
    {									\
      if (t->retval >= ZFS_ERROR_HAS_DC_REPLY)				\
        abort ();							\
      return t->retval;							\
    }									\
                                                                        \
  return zfs_proc_##FUNCTION##_client_1 (t, args, *fd);			\
}
#include "zfs-prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_CLIENT

/*! Return string describing error code.  */

const char *zfs_strerror(int32_t errnum)
{
	if (errnum >= 0)
		return strerror(errnum);

	switch (errnum)
	{
	case ZFS_REQUEST_TOO_LONG:
		return "Request too long";

	case ZFS_INVALID_REQUEST:
		return "Invalid request";

	case ZFS_UNKNOWN_FUNCTION:
		return "Unknown function";

	case ZFS_INVALID_AUTH_LEVEL:
		return "Invalid authentication level";

	case ZFS_STALE:
		return "Stale ZFS file handle";

	case ZFS_METADATA_ERROR:
		return "Metadata error";

	case ZFS_UPDATE_FAILED:
		return "Update failed";

	case ZFS_INVALID_REPLY:
		return "Invalid reply";

	case ZFS_EXITING:
		return "zfsd is exiting";

	case ZFS_COULD_NOT_CONNECT:
		return "Could not connect";

	case ZFS_COULD_NOT_AUTH:
		return "Could not authenticate";

	case ZFS_CONNECTION_CLOSED:
		return "Connection closed";

	case ZFS_REQUEST_TIMEOUT:
		return "Request timed out";

	default:
		return "UNKNOWN error code";
	}

	/* Never reached, just avoids compiler warning.  */
	return "UNKNOWN error code";
}

/*! Call statistics.  */
uint64_t call_statistics[ZFS_PROC_LAST_AND_UNUSED];

/*! Initialize data structures needed by this module.  */

void initialize_zfs_prot_c(void)
{
	int i;

	for (i = 0; i < ZFS_PROC_LAST_AND_UNUSED; i++)
		call_statistics[i] = 0;
}

/*! Cleanup data structures needed by this module.  */

void cleanup_zfs_prot_c(void)
{

#ifdef ENABLE_STATISTICS
	message(LOG_DEBUG, FACILITY_NET, "Call statistics:\n"
			"%-16s%15s%15s\n", "Function", "From kernel", "From network");

#define ZFS_CALL_SERVER
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS, AUTH, CALL_MODE)	\
  if (call_statistics[CALL_FROM_NETWORK][NUMBER] > 0)			\
    {									\
      message (LOG_DEBUG, FACILITY_NET, "%-16s%15" PRIu64 "\n", #FUNCTION,		\
              call_statistics[CALL_FROM_NETWORK][NUMBER]);		\
    }
#include "zfs-prot.def"
#undef DEFINE_ZFS_PROC
#undef ZFS_CALL_SERVER

#endif
}
