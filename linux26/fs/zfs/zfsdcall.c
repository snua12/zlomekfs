/*
   Communication protocol between this kernel module and zfsd.
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
#include "zfsd/data-coding.h"
#include "zfsd/zfs_prot.h"


int zfsd_call(void) {
	DECLARE_WAITQUEUE(wait, current);
	struct request req;
	long timeout_left;
	int error = 0;

	down(&channel.lock);

	if (!channel.connected) {
		error = -EIO;
		goto OUT;
	}

	req.id = channel.request_id++;

	up(&channel.lock);

	req.dc = dc_get();
	if (!req.dc)
		return -ENOMEM;

	start_encoding(req.dc);
	encode_direction(req.dc, DIR_REQUEST);
	encode_request_id(req.dc, req.id);

	/* TODO: encode function code and args */

	req.length = finish_encoding(req.dc);

	down(&channel.lock);

	if (!channel.connected) {
		dc_put(req.dc, 1);
		error = -EIO;
		goto OUT;
	}

	list_add_tail(&req.item, &channel.req_pending);
	req.state = REQ_PENDING;

	wake_up(&channel.waitq);

	up(&channel.lock);

	TRACE("zfs: zfsd_call: %u: request sent\n"
	      "zfs: zfsd_call: %u: sleep\n", req.id, req.id);

	init_waitqueue_head(&req.waitq);
	add_wait_queue(&req.waitq, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	timeout_left = schedule_timeout(ZFS_TIMEOUT * HZ);
	remove_wait_queue(&req.waitq, &wait);

	TRACE("zfs: zfsd_call: %u: wake up\n", req.id);

	down(&channel.lock);

	if (signal_pending(current) || !channel.connected || !timeout_left) {
		switch (req.state) {
			case REQ_PENDING:
				list_del(&req.item);
				dc_put(req.dc, !channel.connected);
				break;
			case REQ_PROCESSING:
				list_del(&req.item);
				break;
			case REQ_REPLY:
				dc_put(req.dc, !channel.connected);
				break;
		}
		if (!channel.connected) {
			TRACE("zfs: zfsd_call: %u: zfsd closed communication device\n", req.id);
			error = -EIO;
		} else if (!timeout_left) {
			TRACE("zfs: zfsd_call: %u: timeout\n", req.id);
			error = -ESTALE;
		} else {
			TRACE("zfs: zfsd_call: %u: interrupt\n", req.id);
			error = -EINTR;
		}
		goto OUT;
	}

	up(&channel.lock);

	TRACE("zfs: zfsd_call: %u: reply received\n", req.id);

	/* TODO: Now we have the reply in req.dc; all to request_id (including) is already decoded. */

	return 0;

OUT:
	up(&channel.lock);
	return error;
}

