/*
   Module definitions.
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

#ifndef _ZFS_H
#define _ZFS_H

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <asm/semaphore.h>

#include "constant.h"
#include "data-coding.h"
#include "zfs_prot.h"


#define ERROR(x...) printk(KERN_ERR x)
#define WARN(x...) printk(KERN_WARNING x)
#define INFO(x...) printk(KERN_INFO x)

#ifdef DEBUG
#define TRACE(x...) printk(KERN_INFO x)
#else
#define TRACE(...)
#endif

#define ZFS_MAGIC *((uint32_t *)"zfs")
#define ZFS_CHARDEV_MAJOR 251

#define ZFS_TIMEOUT (REQUEST_TIMEOUT + 5)

#define ZFS_I(inode) ((struct zfs_inode_info *)inode)
struct zfs_inode_info {
	struct inode vfs_inode;
	zfs_fh fh;
};

#define REQ_PROCESSING_TABSIZE 32
#define INDEX(key) (key % REQ_PROCESSING_TABSIZE)
extern struct channel {
	struct semaphore lock;
	int connected;

	struct semaphore request_id_lock;
	uint32_t request_id;

	struct semaphore req_pending_lock;
	struct list_head req_pending;	/* queue of requests which have been
					   prepared but not sent to zfsd yet */

	struct semaphore req_processing_lock;
	struct list_head req_processing[REQ_PROCESSING_TABSIZE];
					/* hashtable of requests which have
					   been sent to zfsd but corresponding
					   response has not been received */

	wait_queue_head_t waitq;	/* wait queue of zfsd threads which
					   want to receive request but none
					   is prepared */
} channel;

enum request_state {REQ_PENDING, REQ_PROCESSING, REQ_DEQUEUED};

struct request {
	struct semaphore lock;
	enum request_state state;
	unsigned int id;		/* unique request id */
	DC *dc;				/* the message */
	unsigned int length;		/* length of request body (dc.buffer) */
	struct list_head item;		/* item in req_pending
					   or req_processing[] list */
	wait_queue_head_t waitq;	/* wait queue of kernel threads (actualy
					   only current thread) which have
					   prepared this request for zfsd but
					   not received corresponding reply */
};

#endif
