/* Wrappers for pthread_* functions and related data.
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

#ifndef PTHREAD_H
#define PTHREAD_H

#include "system.h"
#include <pthread.h>
#include <string.h>

/* Static mutex initializer.  */
extern pthread_mutex_t zfsd_mutex_initializer;

#define zfsd_mutex_init(M) ((*(M) = zfsd_mutex_initializer), 0)
#define zfsd_cond_init(C) pthread_cond_init (C, NULL)

/* Macros for debugging synchonization primitives.  */
#if defined(ENABLE_CHECKING) && (GCC_VERSION > 2007)

#include <stdio.h>
#include "log.h"

#define zfsd_mutex_destroy(M) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "MUTEX %p DESTROY, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_mutex_destroy (M)) != 0)				\
      {									\
	message (2, stderr, "pthread_mutex_destroy: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

#define zfsd_mutex_lock(M) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "MUTEX %p LOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_mutex_lock (M)) != 0)				\
      {									\
	message (2, stderr, "pthread_mutex_lock: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    message (4, stderr, "MUTEX %p LOCKED, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    0; })

#define zfsd_mutex_unlock(M) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "MUTEX %p UNLOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_mutex_unlock (M)) != 0)				\
      {									\
	message (2, stderr, "pthread_mutex_unlock: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_destroy(C) __extension__				\
  ({									\
    int r;								\
									\
    if ((r = pthread_cond_destroy (C)) != 0)				\
      {									\
	message (2, stderr, "pthread_cond_destroy: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_wait(C, M) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "COND %p WAIT with MUTEX %p, by %lu at %s:%d\n",\
	     (void *) C, (void *) M,					\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_cond_wait (C, M)) != 0)				\
      {									\
	message (2, stderr, "pthread_cond_wait: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_signal(C) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "COND %p SIGNAL, by %lu at %s:%d\n",		\
	     (void *) C,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_cond_signal (C)) != 0)				\
      {									\
	message (2, stderr, "pthread_cond_signal: %d = %s\n",		\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_broadcast(C) __extension__				\
  ({									\
    int r;								\
									\
    message (4, stderr, "COND %p BROADCAST, by %lu at %s:%d\n",		\
	     (void *) C,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((r = pthread_cond_broadcast (C)) != 0)				\
      {									\
	message (2, stderr, "pthread_cond_broadcast: %d = %s\n",	\
		 r, strerror (r));					\
	abort ();							\
      }									\
    0; })

/* Check whether the mutex M is locked.  */
#define CHECK_MUTEX_LOCKED(M)						\
  do {									\
    message (4, stderr, "MUTEX %p TRYLOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((M) && pthread_mutex_trylock (M) == 0)				\
      abort ();								\
  } while (0)

#else

#define zfsd_mutex_destroy(M) pthread_mutex_destroy (M)
#define zfsd_mutex_lock(M) pthread_mutex_lock (M)
#define zfsd_mutex_unlock(M) pthread_mutex_unlock (M)
#define zfsd_cond_destroy(C) pthread_cond_destroy (C)
#define zfsd_cond_wait(C, M) pthread_cond_wait (C, M)
#define zfsd_cond_signal(C) pthread_cond_signal (C)
#define zfsd_cond_broadcast(C) pthread_cond_broadcast (C)
#define CHECK_MUTEX_LOCKED(M)

#endif

#endif
