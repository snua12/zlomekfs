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
  CONNECTION_SLOW,
  CONNECTION_FAST
} connection_status;

/* Status of authentication.  */
typedef enum authentication_status_def
{
  AUTHENTICATION_NONE = 0,
  AUTHENTICATION_IN_PROGRESS,
  AUTHENTICATION_FINISHED
} authentication_status;

/* Data for a network socket.  */
typedef struct network_fd_data_def
{
  pthread_mutex_t mutex;
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
  unsigned int flags;		/* See NETWORK_FD_* below */
} network_fd_data_t;

#define NETWORK_FD_CLOSE 1

/* Pool of network threads.  */
extern thread_pool network_pool;

/* The array of data for each file descriptor.  */
extern network_fd_data_t *network_fd_data;

struct thread_def;

extern void update_node_fd (node nod, int fd, unsigned int generation);
extern void close_network_fd (int fd);
extern bool node_has_valid_fd (node nod);
extern int node_connect_and_authenticate (thread *t, node nod);
extern void recycle_dc_to_fd_data (DC *dc, network_fd_data_t *fd_data);
extern void recycle_dc_to_fd (DC *dc, int fd);
extern void network_worker_init (struct thread_def *t);
extern void network_worker_cleanup (void *data);
extern void add_fd_to_active (int fd);
extern void send_request (struct thread_def *t, uint32_t request_id, int fd);
extern bool network_start ();
extern void network_cleanup ();

#endif
