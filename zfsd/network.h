/* Network thread functions.
   Copyright (C) 2003, 2004 Josef Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef NETWORK_H
#define NETWORK_H

#include "system.h"
#include <inttypes.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "data-coding.h"
#include "hashtab.h"
#include "fibheap.h"
#include "alloc-pool.h"
#include "thread.h"
#include "node.h"
#include "volume.h"

/* Connection status.  */
typedef enum connection_status_def
{
  CONNECTION_NONE = 0,
  CONNECTION_CONNECTING,
  CONNECTION_ACTIVE,
  CONNECTION_PASSIVE,
  CONNECTION_ESTABLISHED
} connection_status;

/* Status of authentication.  */
typedef enum authentication_status_def
{
  AUTHENTICATION_NONE = 0,
  AUTHENTICATION_Q1,
  AUTHENTICATION_STAGE_1,
  AUTHENTICATION_Q3,
  AUTHENTICATION_FINISHED
} authentication_status;

/* Data for a file descriptor used to communicate with other nodes
   or kernel.  */
typedef struct fd_data_def
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  htab_t waiting4reply;		/* table of waiting4reply_data */
  alloc_pool waiting4reply_pool;/* pool of waiting4reply_data */
  fibheap waiting4reply_heap;	/* heap for waiting4reply_data */
  int fd;			/* file descriptor of the socket */
  unsigned int read;		/* number of bytes already read */

  /* Unused data coding buffers for the file descriptor.  */
  DC dc[MAX_FREE_BUFFERS_PER_ACTIVE_FD];
  int ndc;

  time_t last_use;		/* time of last use of the socket */
  unsigned int generation;	/* generation of open file descriptor */
  connection_status conn;	/* status of connection with remote node */
  authentication_status auth;	/* status of authentication with remote node */
  unsigned int sid;		/* ID of node which wants to connect */
  unsigned int busy;		/* number of threads using file descriptor */
  unsigned int flags;		/* See FD_FLAG_* below */
} fd_data_t;

#define FD_FLAG_CLOSE 1

/* Pool of network threads.  */
extern thread_pool network_pool;

/* The array of data for each file descriptor.  */
extern fd_data_t *fd_data_a;

/* Hash function for request ID.  */
#define WAITING4REPLY_HASH(REQUEST_ID) (REQUEST_ID)

struct thread_def;

extern hash_t waiting4reply_hash (const void *xx);
extern int waiting4reply_eq (const void *xx, const void *yy);
extern void update_node_fd (node nod, int fd, unsigned int generation,
			    bool active);
extern void wake_all_threads (fd_data_t *fd_data, int32_t retval);
extern void close_network_fd (int fd);
extern bool node_has_valid_fd (node nod);
extern bool volume_master_connected (volume vol);
extern int node_connect_and_authenticate (thread *t, node nod,
					  authentication_status auth);
extern bool request_from_this_node ();
extern void recycle_dc_to_fd_data (DC *dc, fd_data_t *fd_data);
extern void recycle_dc_to_fd (DC *dc, int fd);
extern void network_worker_init (struct thread_def *t);
extern void network_worker_cleanup (void *data);
extern void add_fd_to_active (int fd);
extern void send_request (struct thread_def *t, uint32_t request_id, int fd);
extern void fd_data_init ();
extern void fd_data_shutdown ();
extern void fd_data_destroy ();
extern bool network_start ();
extern void network_cleanup ();

#endif
