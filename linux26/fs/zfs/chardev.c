/*
   Chardev operations - communication channel between this kernel
   module and ZFSd.
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
#include "data-coding.h"
#include "zfs_prot.h"


static ssize_t zfs_chardev_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
	DECLARE_WAITQUEUE(wait, current);
	struct request *req;
	int error = 0;

	TRACE("zfs:   chardev_read: %u: reading %u bytes\n", current->pid, nbytes);

	/* Wait for request. */
	down(&channel.req_pending_lock);
NEXT_REQUEST:
	while (list_empty(&channel.req_pending)) {
		up(&channel.req_pending_lock);

		TRACE("zfs:   chardev_read: %u: sleep\n", current->pid);

		add_wait_queue_exclusive(&channel.waitq, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		remove_wait_queue(&channel.waitq, &wait);

		TRACE("zfs:   chardev_read: %u: wake up\n", current->pid);

		if (signal_pending(current)) {
			TRACE("zfs:   chardev_read: %u: interrupt\n", current->pid);
			return -EINTR;
		}
		if (!channel.connected) {
			TRACE("zfs:   chardev_read: %u: zfsd closed communication device\n", current->pid);
			return -EIO;
		}

		down(&channel.req_pending_lock);
	}

	req = list_entry(channel.req_pending.next, struct request, item);

	if (down_trylock(&req->lock))
		goto NEXT_REQUEST;

	list_del(&req->item);

	up(&channel.req_pending_lock);

  	if (req->length > nbytes) {
		WARN("zfs: chardev_read: %u: zfsd read only %u bytes of %u in message\n", current->pid, nbytes, req->length);
		req->length = nbytes;
	} else
		TRACE("zfs:   chardev_read: %u: %u bytes read\n", current->pid, req->length);

	if (copy_to_user(buf, req->dc->buffer, req->length))
		error = -EFAULT;

	down(&channel.req_processing_lock);
	list_add_tail(&req->item, &channel.req_processing[INDEX(req->id)]);
	up(&channel.req_processing_lock);

	req->state = REQ_PROCESSING;

	if (!error)
		error = req->length;

	up(&req->lock);

	return error;
}

static ssize_t zfs_chardev_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
	struct list_head *item;
	struct request *req = NULL;
	DC *dc;
	direction dir;
	unsigned int id;

	TRACE("zfs:   chardev_write: writting %u bytes\n", nbytes);

	if (nbytes > DC_SIZE) {
		WARN("zfs: chardev_write: zfsd has written %u bytes but max. %u is allowed in message\n", nbytes, DC_SIZE);
		return -EINVAL;
	}

	dc = dc_get();
	if (!dc)
		return -ENOMEM;

	if (copy_from_user(dc->buffer, buf, nbytes))
		return -EFAULT;

	if (!start_decoding(dc)
	    || !decode_direction(dc, &dir)
	    || !decode_request_id(dc, &id))
		return -EINVAL;

	if (dir == DIR_REQUEST) {
		/* TODO: zfsd wants something, we must reply with the same id */
	} else {
		/* Find the request this reply belongs to. */
		down(&channel.req_processing_lock);
		list_for_each(item, &channel.req_processing[INDEX(id)]) {
			req = list_entry(item, struct request, item);
			if (down_trylock(&req->lock))
				continue;
			if (id == req->id) {
				TRACE("zfs:   chardev_write: request corresponding to reply id %u found\n", id);

				list_del(item);
				up(&channel.req_processing_lock);

				req->state = REQ_DEQUEUED;

				dc_put(req->dc);
				req->dc = dc;

				/* Wait untill the thread waiting for this reply goes to sleep. */
				down(&req->wake_up_lock);

				/* Wake up the thread. */
				wake_up(&req->waitq);

				up(&req->lock);

				return nbytes;
			}
			up(&req->lock);
		}
		up(&channel.req_processing_lock);

		WARN("zfs: chardev_write: no request corresponding to reply id %u found\n", id);
	}

	dc_put(dc);

	return nbytes;
}

static int zfs_chardev_open(struct inode *inode, struct file *file)
{
	int i;

	TRACE("zfs:   chardev_open\n");

	down(&channel.lock);

	if (channel.connected) {
		up(&channel.lock);
		return -EBUSY;
	}

	init_MUTEX(&channel.request_id_lock);
	channel.request_id = 0;

	init_MUTEX(&channel.req_pending_lock);
	INIT_LIST_HEAD(&channel.req_pending);

	init_MUTEX(&channel.req_processing_lock);
	for (i = 0; i < REQ_PROCESSING_TABSIZE; i++)
		INIT_LIST_HEAD(&channel.req_processing[i]);

	init_waitqueue_head(&channel.waitq);

	channel.connected = 1;

	up(&channel.lock);

	return 0;
}

static int zfs_chardev_release(struct inode *inode, struct file *file)
{
	struct list_head *item;
	struct request *req;
	int i;

	TRACE("zfs:   chardev_close\n");

	down(&channel.lock);

	channel.connected = 0;

	down(&channel.req_pending_lock);
	list_for_each(item, &channel.req_pending) {
		req = list_entry(item, struct request, item);
		wake_up(&req->waitq);
	}
	up(&channel.req_pending_lock);

	down(&channel.req_processing_lock);
	for (i = 0; i < REQ_PROCESSING_TABSIZE; i++)
		list_for_each(item, &channel.req_processing[i]) {
			req = list_entry(item, struct request, item);
			wake_up(&req->waitq);
		}
	up(&channel.req_processing_lock);

	wake_up_all(&channel.waitq);

	up(&channel.lock);

	dc_destroy_all();

	return 0;
}

struct file_operations zfs_chardev_file_operations = {
	.owner		= THIS_MODULE,
	.read		= zfs_chardev_read,
	.write		= zfs_chardev_write,
	.open		= zfs_chardev_open,
	.release	= zfs_chardev_release,
};
