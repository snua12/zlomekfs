/*
   Chardev operations - communication channel between this kernel
   module and zfsd.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "zfs.h"
#include "zfsd/data-coding.h"
#include "zfsd/zfs_prot.h"


static int zfs_chardev_open(struct inode *inode, struct file *file)
{
	int i;

	TRACE("zfs: chardev_open\n");

	down(&channel.lock);

	if (channel.connected) {
		up(&channel.lock);
		return -EBUSY;
	}

	channel.connected = 1;
	channel.request_id = 0;
	INIT_LIST_HEAD(&channel.req_pending);
	for (i = 0; i < REQ_PROCESSING_TABSIZE; i++)
		INIT_LIST_HEAD(&channel.req_processing[i]);
	init_waitqueue_head(&channel.waitq);

	up(&channel.lock);

	return 0;
}

static int zfs_chardev_release(struct inode *inode, struct file *file)
{
	struct list_head *item;
	struct request *req;
	int i;

	TRACE("zfs: chardev_close\n");

	down(&channel.lock);

	channel.connected = 0;
	list_for_each(item, &channel.req_pending) {
		req = list_entry(item, struct request, item);
		wake_up(&req->waitq);
	}
	for (i = 0; i < REQ_PROCESSING_TABSIZE; i++)
		__list_for_each(item, &channel.req_processing[i]) {
			req = list_entry(item, struct request, item);
			wake_up(&req->waitq);
		}
	wake_up_all(&channel.waitq);

	up(&channel.lock);

	dc_destroy_all();

	return 0;
}

int zfsd_call(void);

static ssize_t zfs_chardev_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
	DECLARE_WAITQUEUE(wait, current);
	struct request *req = NULL;
	int error = 0;

	TRACE("zfs: chardev_read: %u bytes\n", nbytes);

	down(&channel.lock);

	__add_wait_queue_tail(&channel.waitq, &wait);
	while (list_empty(&channel.req_pending)) {
		up(&channel.lock);

		TRACE("zfs: chardev_read: sleep\n");

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		TRACE("zfs: chardev_read: wake up\n");

		down(&channel.lock);

		if (signal_pending(current)) {
			TRACE("zfs: chardev_read: interrupt\n");
			error = -EINTR;
			break;
		}
		if (!channel.connected) {
			TRACE("zfs: chardev_read: zfsd closed communication device\n");
			error = -EIO;
			break;
		}
	}
	remove_wait_queue(&channel.waitq, &wait);

	if (error) {
		up(&channel.lock);
		return error;
	}

	req = list_entry(channel.req_pending.next, struct request, item);
	list_del(&req->item);

  	if (req->length > nbytes) {
		WARN("zfs: chardev_read: zfsd read only %u bytes of %u in message\n", nbytes, req->length);
		req->length = nbytes;
	}

	if (copy_to_user(buf, req->dc->buffer, req->length))
		error = -EFAULT;

	list_add_tail(&req->item, &channel.req_processing[INDEX(req->id)]);
	req->state = REQ_PROCESSING;

	up(&channel.lock);

	dc_put(req->dc, 0);

	return error ? error : req->length;
}

static ssize_t zfs_chardev_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
	struct list_head *item;
	struct request *req = NULL;
	DC *dc;
	direction dir;
	uint32_t id;

	TRACE("zfs: chardev_write: %u bytes\n", nbytes);

	if (nbytes > DC_SIZE) {
		WARN("zfs: chardev_write: zfsd has written %u bytes but max. %u is allowed in message\n", nbytes, DC_SIZE);
		return -EINVAL;
	}

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	if (copy_from_user(dc->buffer, buf, nbytes)) {
		dc_put(dc, 0);
		return -EFAULT;
	}

	if (!start_decoding(dc)
	    || !decode_direction(dc, &dir)
	    || !decode_request_id(dc, &id)) {
		dc_put(dc, 0);
		return -EINVAL;
	}

	if (dir == DIR_REQUEST) {
		/* TODO: zfsd wants something, we must reply with the same id */
	} else {
		down(&channel.lock);

		__list_for_each(item, &channel.req_processing[INDEX(id)]) {
			req = list_entry(item, struct request, item);
			if (id == req->id) {
				list_del(item);
				req->dc = dc;
				req->state = REQ_REPLY;
				wake_up(&req->waitq);
				break;
			}
		}

		up(&channel.lock);

		if (req && (id == req->id))
			TRACE("zfs: chardev_write: request corresponding to reply id %u found\n", id);
		else
			WARN("zfs: chardev_write: no request corresponding to reply id %u found\n", id);
	}

	return nbytes;
}

struct file_operations zfs_chardev_file_operations = {
	.owner		= THIS_MODULE,
	.open		= zfs_chardev_open,
	.release	= zfs_chardev_release,
	.read		= zfs_chardev_read,
	.write		= zfs_chardev_write,
};

