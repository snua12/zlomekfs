/*
   Chardev operations - communication channel between this kernel
   module and ZFSd.
   Copyright (C) 2004 Martin Zlomek

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
#include <linux/errno.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "zfs.h"
#include "data-coding.h"
#include "zfs_prot.h"


static ssize_t zfs_chardev_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
	struct request *req;
	int error = 0;

	TRACE("%u: reading %u bytes (going to sleep if no data avaible)", current->pid, nbytes);

NEXT_REQUEST:
	/* Wait for a request. */
	down_interruptible(&channel.req_pending_count);

	if (signal_pending(current)) {
		TRACE("%u: interrupt", current->pid);
		return -EINTR;
	}
	if (!channel.connected) {
		TRACE("%u: zfsd closed communication device", current->pid);
		return -EIO;
	}

	down(&channel.req_pending_lock);
	req = list_entry(channel.req_pending.next, struct request, item);
	if (down_trylock(&req->lock)) {
		up(&channel.req_pending_lock);
		goto NEXT_REQUEST;
	}
	list_del(&req->item);
	up(&channel.req_pending_lock);

  	if (req->length > nbytes) {
		WARN("%u: zfsd read only %u bytes of %u in message", current->pid, nbytes, req->length);
		req->length = nbytes;
	} else
		TRACE("%u: %u bytes read", current->pid, req->length);

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

	TRACE("%u: writting %u bytes", current->pid, nbytes);

	if (nbytes > DC_SIZE) {
		WARN("%u: zfsd has written %u bytes but max. %u is allowed in message", current->pid, nbytes, DC_SIZE);
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
		/* TODO: Zfsd wants something, we must reply with the same id. */
	} else {
		/* Find the request this reply belongs to. */
		down(&channel.req_processing_lock);
		list_for_each(item, &channel.req_processing[INDEX(id)]) {
			req = list_entry(item, struct request, item);
			if (down_trylock(&req->lock))
				continue;
			if (id == req->id) {
				TRACE("%u: request corresponding to reply id %u found", current->pid, id);

				list_del(item);
				up(&channel.req_processing_lock);

				req->state = REQ_DEQUEUED;

				dc_put(req->dc);
				req->dc = dc;

				/* Wake up the thread which is waiting for this reply. */
				wake_up(&req->waitq);

				up(&req->lock);

				return nbytes;
			}
			up(&req->lock);
		}
		up(&channel.req_processing_lock);

		WARN("%u: no request corresponding to reply id %u found", current->pid, id);
	}

	dc_put(dc);

	return nbytes;
}

static int zfs_chardev_open(struct inode *inode, struct file *file)
{
	int i;

	TRACE("%u", current->pid);

	down(&channel.lock);

	if (channel.connected) {
		up(&channel.lock);
		return -EBUSY;
	}

	init_MUTEX(&channel.request_id_lock);
	channel.request_id = 0;

	init_MUTEX_LOCKED(&channel.req_pending_count);

	init_MUTEX(&channel.req_pending_lock);
	INIT_LIST_HEAD(&channel.req_pending);

	init_MUTEX(&channel.req_processing_lock);
	for (i = 0; i < REQ_PROCESSING_TABSIZE; i++)
		INIT_LIST_HEAD(&channel.req_processing[i]);

	channel.connected = 1;

	up(&channel.lock);

	return 0;
}

static int zfs_chardev_release(struct inode *inode, struct file *file)
{
	struct list_head *item;
	struct request *req;
	int i;

	TRACE("%u", current->pid);

	down(&channel.lock);

	channel.connected = 0;

	wake_up_all(&channel.req_pending_count.wait);
	/* No up(&req_pending_count) is neccessary - the samaphore will be initialized in the next call of zfs_chardev_open(). */

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
