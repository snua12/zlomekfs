/* Server thread functions.
   Copyright (C) 2003 Josef Zlomek

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

#ifndef SERVER_H
#define SERVER_H

#include "system.h"
#include <time.h>
#include "constant.h"
#include "data-coding.h"
#include "hashtab.h"
#include "alloc-pool.h"
#include "node.h"

/* Data for a server socket.  */
typedef struct server_fd_data_def
{
  pthread_mutex_t mutex;
  htab_t waiting4reply;		/* table of waiting4reply_data */
  alloc_pool waiting4reply_pool;/* pool of waiting4reply_data */
  int fd;			/* file descriptor of the socket */
  unsigned int read;		/* number of bytes already read */

  /* Unused data coding buffers for the file descriptor.  */
  DC dc[MAX_FREE_BUFFERS_PER_SERVER_FD];
  int ndc;

  time_t last_use;		/* time of last use of the socket */
  unsigned int generation;	/* generation of open file descriptor */
  authentication_status auth;	/* status of authentication with remote node */
  int busy;			/* number of threads using file descriptor */
} server_fd_data_t;

/* Additional data for a server thread.  */
typedef struct server_thread_data_def
{
#ifdef RPC
  /* Parameters passed to zfs_program_1 */
  struct svc_req *rqstp;
  SVCXPRT *transp;
#else
  server_fd_data_t *fd_data;	/* passed from main server thread */
  DC dc;			/* buffer for request to this server */
  DC dc_call;			/* buffer for request for remote server */
  unsigned int generation;	/* generation of file descriptor */
  int index;			/* index of FD in array "active" */
  call_args args;		/* union for decoded call arguments.  */
  int retval;			/* return value for request.  */
#endif
} server_thread_data;

#define SERVER_ANY 0

#ifndef RPC

/* Thread ID of the main server thread (thread receiving data from sockets).  */
extern pthread_t main_server_thread;

/* The array of data for each file descriptor.  */
extern server_fd_data_t *server_fd_data;

#endif

struct thread_def;

extern void close_server_fd (int fd);
extern void server_worker_init (struct thread_def *t);
extern void server_worker_cleanup (void *data);
extern void add_fd_to_active (int fd, node nod);
extern void send_request (struct thread_def *t, uint32_t request_id, int fd);
extern bool create_server_threads ();
#ifdef RPC
extern void register_server ();
#else
extern bool server_start ();
extern bool server_init_fd_data ();
#endif
extern void server_cleanup ();

#endif
