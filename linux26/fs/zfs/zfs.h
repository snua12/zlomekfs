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

#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <asm/semaphore.h>

#include "zfsd/constant.h"
#include "zfsd/data-coding.h"


#define ZFS_DEBUG

#define ERROR(x...) printk(KERN_ERR x)
#define WARN(x...) printk(KERN_WARNING x)

#ifdef ZFS_DEBUG
#define TRACE(x...) printk(KERN_INFO x)
#else
#define TRACE(...)
#endif

#define ZFS_MAGIC *((uint32_t *)"zfs")
#define ZFS_CHARDEV_MAJOR 251

#define ZFS_TIMEOUT (REQUEST_TIMEOUT + 5)

#define REQ_PROCESSING_TABSIZE 32
#define INDEX(key) (key % REQ_PROCESSING_TABSIZE)

extern struct channel {
	struct semaphore lock;
	int connected;
	uint32_t request_id;

	struct list_head req_pending;	/* queue of requests which have been
					   prepared but not sent to zfsd yet */
	struct list_head req_processing[REQ_PROCESSING_TABSIZE];
					/* hashtable of requests which have
					   been sent to zfsd but corresponding
					   response has not been received */
	wait_queue_head_t waitq;	/* wait queue of zfsd threads which
					   want to receive request but none
					   is prepared */
} channel;

enum request_state {REQ_PENDING, REQ_PROCESSING, REQ_REPLY};

struct request {
	enum request_state state;
	uint32_t id;			/* unique request id */
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
