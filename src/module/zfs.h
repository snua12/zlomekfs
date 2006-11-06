/*
   Module definitions.
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

#ifndef _ZFS_H
#define _ZFS_H

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <asm/semaphore.h>

#include "constant.h"
#include "data-coding.h"
#include "zfs-prot.h"


#define ERROR(format, ...) printk(KERN_ERR "zfs: " format "\n", ##__VA_ARGS__)
#define WARN(format, ...) printk(KERN_WARNING "zfs: " format "\n", ##__VA_ARGS__)
#define INFO(format, ...) printk(KERN_INFO "zfs: " format "\n", ##__VA_ARGS__)

#ifdef DEBUG
# define TRACE(format, ...) printk(KERN_INFO "zfs: %s(): " format "\n", __func__, ##__VA_ARGS__)
#else
# define TRACE(...)
#endif

#define ZFS_SUPER_MAGIC *((uint32_t *)"zfs")
#define ZFS_CHARDEV_MAJOR 251

/* Timeout in seconds for request. */
#define ZFS_TIMEOUT (REQUEST_TIMEOUT + 5)

/* Maximum age of dentry in seconds after that revalidation is requered. */
#define ZFS_DENTRY_MAXAGE 5

#define CAP(p) ((zfs_cap *)p)
#define COOKIE(p) ((int32_t *)&((zfs_cap *)p)[1])

/* Hash function which returns (not neccessary unique) inode number. */
#define ROTATE_LEFT(x, nbites) ((x << nbites) | (x >> (32 - nbites)))
#define HASH(fh) (ROTATE_LEFT(fh->sid, 22) ^ ROTATE_LEFT(fh->dev, 12) ^ fh->ino)

/* ZFS super block. */
extern struct super_block *zfs_sb;

/* ZFS inode info flags. */
#define NEED_REVALIDATE 0x01

/* ZFS inode info. */
#define ZFS_I(inode) ((struct zfs_inode_info *)inode)
struct zfs_inode_info {
        struct inode vfs_inode;
        zfs_fh fh;
        int flags;
        zfs_cap *cap;
};

/* Size of hash table of processing requests. */
#define REQ_PROCESSING_TABSIZE 32

/* Hash function which returns index of queue in the hash table. */
#define INDEX(key) (key % REQ_PROCESSING_TABSIZE)

/* Communication channel between this kernel module and ZFSd. */
extern struct channel {
        struct semaphore lock;
        volatile int connected;

        struct semaphore request_id_lock;
        uint32_t request_id;

        struct semaphore req_pending_count;
                /* count of requests in the req_pending queue */
        struct semaphore req_pending_lock;
        struct list_head req_pending;
                /* queue of requests which have been prepared
                   but not sent to zfsd yet */

        struct semaphore req_processing_lock;
        struct list_head req_processing[REQ_PROCESSING_TABSIZE];
                /* hashtable of requests which have been sent to ZFSd
                   but corresponding reply has not been received yet */
} channel;

enum request_state {REQ_PENDING, REQ_PROCESSING, REQ_DEQUEUED};

/* Request to ZFSd. */
struct request {
        struct semaphore lock;
        enum request_state state;
        unsigned int id;	/* unique request id */
        DC *dc;			/* the message */
        unsigned int length;	/* length of request body (dc.buffer) */
        struct list_head item;	/* item in req_pending
                                   or req_processing[] list */
        wait_queue_head_t waitq;
                /* wait queue of kernel threads (actualy only current thread)
                   which have prepared the request but not received the reply */
};

#endif
