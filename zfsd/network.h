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

/* Data for a server socket.  */
typedef struct server_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor of the socket */
  unsigned int read;		/* number of bytes already read */

  /* Unused data coding buffers for the file descriptor.  */
  DC dc[MAX_FREE_BUFFERS_PER_SERVER_FD];
  int ndc;

  unsigned int length;		/* the length of the request */
  time_t last_use;		/* time of last use of the socket */
  unsigned int generation;	/* generation of open file descriptor */
  int busy;			/* number of threads using file descriptor */
} server_fd_data_t;

#define SERVER_ANY 0

#ifndef RPC
/* Thread ID of the main server thread (thread receiving data from sockets).  */
extern pthread_t main_server_thread;

/* Key for server thread specific data.  */
extern pthread_key_t server_thread_data_key;

#endif

extern int create_server_threads ();
#ifdef RPC
extern void register_server ();
#else
extern int server_start ();
#endif
extern int server_cleanup ();

#endif
