/* Functions for threads communicating with kernel.
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

#ifndef CLIENT_H
#define CLIENT_H

#include "system.h"
#include "pthread.h"
#include "thread.h"

/* Data for kernel pool regulator.  */
extern thread_pool_regulator_data kernel_regulator_data;

/* Thread ID of the main kernel thread (thread receiving data from socket).  */
extern pthread_t main_kernel_thread;

/* This mutex is locked when main kernel thread is in poll.  */
extern pthread_mutex_t main_kernel_thread_in_syscall;

extern bool create_kernel_threads ();
extern bool kernel_start ();
extern void kernel_cleanup ();

#endif
