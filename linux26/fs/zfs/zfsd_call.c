/*
   ZFSd operations.
   Copyright (C) 2004 Antonin Prukl, Miroslav Rudisin, Martin Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/compiler.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "zfs.h"
#include "data-coding.h"
#include "zfs_prot.h"


int send_request(struct request *req) {
	DECLARE_WAITQUEUE(wait, current);
	long timeout_left;

	TRACE("zfs:   send_request: %u\n", req->id);

	if (!channel.connected) {
		TRACE("zfs:   send_request: %u: zfsd closed communication device\n", req->id);
		return -EIO;
	}

	init_MUTEX(&req->lock);
	init_MUTEX_LOCKED(&req->wake_up_lock);
	init_waitqueue_head(&req->waitq);

	down(&channel.req_pending_lock);
	list_add_tail(&req->item, &channel.req_pending);
	up(&channel.req_pending_lock);

	req->state = REQ_PENDING;

	/* Wake up a thread waiting for a request. */
	wake_up(&channel.waitq);

	TRACE("zfs:   send_request: %u: sleep\n", req->id);

	add_wait_queue(&req->waitq, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	/* Allow the thread which receives the reply to wake me up. */
	up(&req->wake_up_lock);

	timeout_left = schedule_timeout(ZFS_TIMEOUT * HZ);
	remove_wait_queue(&req->waitq, &wait);

	TRACE("zfs:   send_request: %u: wake up\n", req->id);

	down(&req->lock);

	switch (req->state) {
		case REQ_PENDING:
			down(&channel.req_pending_lock);
			list_del(&req->item);
			up(&channel.req_pending_lock);
			break;
		case REQ_PROCESSING:
			down(&channel.req_processing_lock);
			list_del(&req->item);
			up(&channel.req_processing_lock);
			break;
		default:
			break;
	}

	if (signal_pending(current)) {
		TRACE("zfs:   send_request: %u: interrupt\n", req->id);
		return -EINTR;
	}
	if (!timeout_left) {
		TRACE("zfs:   send_request: %u: timeout\n", req->id);
		return -ESTALE;
	}
	if (!channel.connected) {
		TRACE("zfs:   send_request: %u: zfsd closed communication device\n", req->id);
		return -EIO;
	}

	TRACE("zfs:   send_request: %u: corresponding reply received\n", req->id);

	return 0;
}

int zfsd_root(zfs_fh *fh)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_root\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_root_zfsd(&dc, NULL);

	if (!error
	    && (!decode_zfs_fh(dc, fh)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_root: %d\n", error);

	return error;
}

int zfsd_getattr(fattr *attr, zfs_fh *fh)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_getattr\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_getattr_zfsd(&dc, fh);

	if (!error
	    && (!decode_fattr(dc, attr)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_getattr: %d\n", error);

	return error;
}

int zfsd_setattr(fattr *attr, sattr_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_setattr\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_setattr_zfsd(&dc, args);

	if (!error
	    && (!decode_fattr(dc, attr)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_setattr: %d\n", error);

	return error;
}

int zfsd_create(create_res *res, create_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_create\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_create_zfsd(&dc, args);

	if (!error
	    && (!decode_create_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_create: %d\n", error);

	return error;
}

int zfsd_lookup(dir_op_res *res, dir_op_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_lookup\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_lookup_zfsd(&dc, args);

	if (!error
	    && (!decode_dir_op_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_lookup: %d\n", error);

	return error;
}

int zfsd_link(link_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_link\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_link_zfsd(&dc, args);

	if (!error
	    && !finish_decoding(dc))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_link: %d\n", error);

	return error;
}

int zfsd_unlink(dir_op_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_unlink\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_unlink_zfsd(&dc, args);

	if (!error
	    && !finish_decoding(dc))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_unlink: %d\n", error);

	return error;
}

int zfsd_symlink(dir_op_res *res, symlink_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_symlink\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_symlink_zfsd(&dc, args);

	if (!error
	    && (!decode_dir_op_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_symlink: %d\n", error);

	return error;
}

int zfsd_mkdir(dir_op_res *res, mkdir_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_mkdir\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_mkdir_zfsd(&dc, args);

	if (!error
	    && (!decode_dir_op_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_mkdir: %d\n", error);

	return error;
}

int zfsd_rmdir(dir_op_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_rmdir\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_rmdir_zfsd(&dc, args);

	if (!error
	    && !finish_decoding(dc))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_rmdir: %d\n", error);

	return error;
}

int zfsd_mknod(dir_op_res *res, mknod_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_mknod\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_mknod_zfsd(&dc, args);

	if (!error
	    && (!decode_dir_op_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_mknod: %d\n", error);

	return error;
}

int zfsd_rename(rename_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_rename\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_rename_zfsd(&dc, args);

	if (!error
	    && !finish_decoding(dc))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_rename: %d\n", error);

	return error;
}

int zfsd_readlink(read_link_res *res, zfs_fh *fh)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_readlink\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_readlink_zfsd(&dc, fh);

	if (!error
	    && (!decode_read_link_res(dc, res)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_readlink: %d\n", error);

	return error;
}

int zfsd_open(zfs_cap *cap, open_args *args)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_open\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_open_zfsd(&dc, args);

	if (!error
	    && (!decode_zfs_cap(dc, cap)
		|| !finish_decoding(dc)))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_open: %d\n", error);

	return error;
}

int zfsd_close(zfs_cap *cap)
{
	DC *dc;
	int error;

	TRACE("zfs:  zfsd_close\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_close_zfsd(&dc, cap);

	if (!error
	    && !finish_decoding(dc))
		error = -EPROTO;

	dc_put(dc);

	TRACE("zfs:  zfsd_close: %d\n", error);

	return error;
}

int zfsd_readdir(read_dir_args *args, struct file *file, void *dirent, filldir_t filldir)
{
	DC *dc;
	dir_list list;
	dir_entry entry;
	struct qstr name;
	int error, entries = 0, i;

	TRACE("zfs:  zfsd_readdir\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	while (1) {
		error = zfs_proc_readdir_zfsd(&dc, args);
		if (error)
			break;

		if (!decode_dir_list(dc, &list)) {
			error = -EPROTO;
			break;
		}

		for (i = 0; i < list.n; i++) {
			if (!decode_dir_entry(dc, &entry)) {
				error = -EPROTO;
				break;
			}

			TRACE("zfs:  zfsd_readdir: entry: ino=%u, cookie=%d, '%s'\n", entry.ino, entry.cookie, entry.name.str);

			name.name = entry.name.str;
			name.len = entry.name.len;

			error = filldir(dirent, name.name, name.len, file->f_pos, entry.ino, DT_UNKNOWN);
			if (error)
				break;

			file->f_pos = entry.cookie;
			entries++;
		}
		if (error)
			break;

		if (!finish_decoding(dc)) {
			error = -EPROTO;
			break;
		}

		if (list.eof)
			break;

		args->cookie = entry.cookie;
	}

	dc_put(dc);

	TRACE("zfs:  zfsd_readdir: %d\n", entries ? entries : error);

	return entries ? entries : error;
}

int zfsd_read(char __user *buf, read_args *args)
{
	DC *dc;
	uint32_t nbytes;
	int error;

	TRACE("zfs:  zfsd_read: reading %u bytes\n", args->count);

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_read_zfsd(&dc, args);

	if (!error) {
		if (!decode_uint32_t(dc, &nbytes)
		    || (nbytes > args->count))
			error = -EPROTO;
		else {
			dc->cur_length += nbytes;
			if (!finish_decoding(dc))
				error = -EPROTO;
			else {
				if (copy_to_user(buf, dc->cur_pos, nbytes))
					error = -EFAULT;
				else
					error = nbytes;
			}
		}
	}

	dc_put(dc);

	TRACE("zfs:  zfsd_read: %d\n", error);

	return error;
}

int zfsd_write(write_args *args)
{
	DC *dc;
	write_res res;
	int error;

	TRACE("zfs:  zfsd_write: writting %u bytes\n", args->data.len);

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_write_zfsd(&dc, args);

	if (!error) {
		if (!decode_write_res(dc, &res)
		    || (res.written > args->data.len)
		    || !finish_decoding(dc))
			error = -EPROTO;
		else
			error = res.written;
	} else if (dc->cur_pos == NULL)
		error = -EFAULT;

	dc_put(dc);

	TRACE("zfs:  zfsd_write: %d\n", error);

	return error;
}

