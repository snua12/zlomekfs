/* Various constants.
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

#ifndef CONSTANT_H
#define CONSTANT_H

/* The interval between 2 invocations of thread pool regulator in seconds.  */
#define THREAD_POOL_REGULATOR_INTERVAL 15

/* The time between two attempts to connect to node in seconds.  */
#define NODE_CONNECT_VISCOSITY 10

/* Maximal number of free data buffers for an active file descriptor.  */
#define MAX_FREE_BUFFERS_PER_SERVER_FD 4

/* Maximal length of request / reply.  */
#define ZFS_MAX_REQUEST_LEN 8888

/* The event groups for poll().  */
#define CAN_READ (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)
#define CAN_WRITE (POLLOUT | POLLWRNORM | POLLWRBAND)
#define CANNOT_RW (POLLERR | POLLHUP | POLLNVAL)

/* Maximal number of file descriptors.  */
extern int max_nfd;

/* Maximal number of server sockets.  */
extern int max_server_sockets;

extern void init_constants ();

#endif
