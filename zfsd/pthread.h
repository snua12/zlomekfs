/* Wrappers for pthread_* functions.
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

#ifndef PTHREAD_H
#define PTHREAD_H

#include <pthread.h>

/* Macros for debugging synchonization primitives.  */
#ifdef ENABLE_CHECKING

#include <stdio.h>
#include "log.h"

#define zfsd_mutex_lock(M)						\
  do {									\
    message (3, stderr, "MUTEX %p LOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    pthread_mutex_lock (M);						\
    message (3, stderr, "MUTEX %p LOCKED, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
  } while (0)

#define zfsd_mutex_unlock(M)						\
  do {									\
    message (3, stderr, "MUTEX %p UNLOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    pthread_mutex_unlock (M);						\
  } while (0)

#define zfsd_cond_wait(C, M)						\
  do {									\
    message (3, stderr, "COND %p WAIT with MUTEX %p, by %lu at %s:%d\n",\
	     (void *) C, (void *) M,					\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    pthread_cond_wait (C, M);						\
  } while (0)

#define zfsd_cond_signal(C)						\
  do {									\
    message (3, stderr, "COND %p SIGNAL, by %lu at %s:%d\n",		\
	     (void *) C,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    pthread_cond_signal (C);						\
  } while (0)

#else

#define zfsd_mutex_lock(M) pthread_mutex_lock (M)
#define zfsd_mutex_unlock(M) pthread_mutex_unlock (M)
#define zfsd_cond_wait(C, M) pthread_cond_wait (C, M)
#define zfsd_cond_signal(C) pthread_cond_signal (C)

#endif

#endif
