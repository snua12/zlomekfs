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

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/semaphore.h>

#include "zfs.h"
#include "data-coding.h"
#include "zfs_prot.h"


int send_request(struct request *req) {
	DECLARE_WAITQUEUE(wait, current);
	long timeout_left;

	TRACE("zfs: send_request: %u\n", req->id);

	init_MUTEX(&req->lock);
	init_waitqueue_head(&req->waitq);

	down(&channel.req_pending_lock);
	list_add_tail(&req->item, &channel.req_pending);
	up(&channel.req_pending_lock);
	req->state = REQ_PENDING;

	wake_up(&channel.waitq);

	TRACE("zfs: send_request: %u: sleep\n", req->id);

	add_wait_queue(&req->waitq, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	timeout_left = schedule_timeout(ZFS_TIMEOUT * HZ);
	remove_wait_queue(&req->waitq, &wait);

	TRACE("zfs: send_request: %u: wake up\n", req->id);

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
		TRACE("zfs: send_request: %u: interrupt\n", req->id);
		return -EINTR;
	} else if (!timeout_left) {
		TRACE("zfs: send_request: %u: timeout\n", req->id);
		return -ESTALE;
	} else if (req->state != REQ_DEQUEUED) {
		TRACE("zfs: send_request: %u: zfsd closed communication device\n", req->id);
		return -EIO;
	}

	TRACE("zfs: send_request: %u: reply received\n", req->id);

	return 0;
}

int zfsd_root(zfs_fh *fh)
{
	DC *dc;
	int error;

	TRACE("zfs: zfsd_root\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_root_zfsd(&dc, NULL);

	if (!error) {
		if (!decode_zfs_fh(dc, fh)
		    || !finish_decoding(dc))
			error = -EPROTO;
	}

	dc_put(dc);

	return error;
}

int zfsd_getattr(fattr *attr, zfs_fh *fh)
{
	DC *dc;
	int error;

	TRACE("zfs: zfsd_getattr\n");

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	error = zfs_proc_getattr_zfsd(&dc, fh);

	if (!error) {
		if (!decode_fattr(dc, attr)
		    || !finish_decoding(dc))
			error = -EPROTO;
	}

	dc_put(dc);

	return error;
}

