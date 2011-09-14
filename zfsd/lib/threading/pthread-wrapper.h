/* ! \file \brief Wrappers for pthread_* functions and related data.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#ifndef PTHREAD_H
#define PTHREAD_H

#include "system.h"
#include "log.h"

#include <pthread.h>
#include <string.h>

/* ! Static mutex initializer.  */
extern pthread_mutex_t zfsd_mutex_initializer;

#define zfsd_mutex_init(M) ((*(M) = zfsd_mutex_initializer), 0)
#define zfsd_cond_init(C) pthread_cond_init (C, NULL)

#if defined(ENABLE_CHECKING) && (GCC_VERSION > 2007) && defined(PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP)
#  define ENABLE_PTHREAD_CHECKING
#endif

#ifdef ENABLE_PTHREAD_CHECKING

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
	#define ZFS_MUTEX_INITIALIZER PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
	#define ZFS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

#else

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
	#define ZFS_MUTEX_INITIALIZER PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
#else
	#define ZFS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

#endif

/* ! Macros for debugging synchonization primitives.  */
#ifdef ENABLE_PTHREAD_CHECKING

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "log.h"

#define zfsd_mutex_destroy(M) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p DESTROY, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_mutex_destroy (M)) != 0)				\
      {									\
	message (LOG_WARNING, FACILITY_THREADING, "pthread_mutex_destroy: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

#define zfsd_mutex_lock(M) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p LOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_mutex_lock (M)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_mutex_lock: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p LOCKED, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    0; })

#define zfsd_mutex_unlock(M) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p UNLOCK, by %lu at %s:%d\n",		\
	     (void *) M,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_mutex_unlock (M)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_mutex_unlock: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_destroy(C) __extension__				\
  ({									\
    int def_ret;								\
									\
    if ((def_ret = pthread_cond_destroy (C)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_cond_destroy: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_wait(C, M) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "COND %p WAIT with MUTEX %p, by %lu at %s:%d\n",\
	     (void *) C, (void *) M,					\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_cond_wait (C, M)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_cond_wait: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_signal(C) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "COND %p SIGNAL, by %lu at %s:%d\n",		\
	     (void *) C,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_cond_signal (C)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_cond_signal: %d = %s\n",		\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

#define zfsd_cond_broadcast(C) __extension__				\
  ({									\
    int def_ret;								\
									\
    message (LOG_LOCK, FACILITY_THREADING, "COND %p BROADCAST, by %lu at %s:%d\n",		\
	     (void *) C,						\
	     (unsigned long) pthread_self (), __FILE__, __LINE__);	\
    if ((def_ret = pthread_cond_broadcast (C)) != 0)				\
      {									\
	message (LOG_ERROR, FACILITY_THREADING, "pthread_cond_broadcast: %d = %s\n",	\
		 def_ret, strerror (def_ret));					\
	abort ();							\
      }									\
    0; })

/* ! Check whether the mutex M is locked.  */
#define CHECK_MUTEX_LOCKED(M)						\
  ({									\
  do {									\
    if ((M) != NULL)								\
      {									\
									\
	message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p CHECK, by %lu at %s:%d\n",	\
		 (void *) (M),						\
		 (unsigned long) pthread_self (), __FILE__, __LINE__);	\
	if( pthread_mutex_lock (M)!= EDEADLK )						\
	  abort ();							\
      }									\
  } while (0);								\
  })

/* ! Check whether the mutex M is NOT locked by current thread.  */
#define CHECK_MUTEX_UNLOCKED(M)						\
  ({									\
  do {									\
    int _mutex_lock_r = 0;						\
    if ((M) != NULL)								\
      {									\
	message (LOG_LOCK, FACILITY_THREADING, "MUTEX %p CHECK, by %lu at %s:%d\n",	\
		 (void *) M,						\
		 (unsigned long) pthread_self (), __FILE__, __LINE__);	\
	_mutex_lock_r = pthread_mutex_lock (M);				\
	if (_mutex_lock_r == EDEADLK)					\
	  abort ();							\
	if (_mutex_lock_r == 0)						\
	  pthread_mutex_unlock (M);					\
      }									\
  } while (0);								\
  })

#else

#define zfsd_mutex_destroy(M) pthread_mutex_destroy (M)
#define zfsd_mutex_lock(M) pthread_mutex_lock (M)
#define zfsd_mutex_unlock(M) pthread_mutex_unlock (M)
#define zfsd_cond_destroy(C) pthread_cond_destroy (C)
#define zfsd_cond_wait(C, M) pthread_cond_wait (C, M)
#define zfsd_cond_signal(C) pthread_cond_signal (C)
#define zfsd_cond_broadcast(C) pthread_cond_broadcast (C)
#define CHECK_MUTEX_LOCKED(M)
#define CHECK_MUTEX_UNLOCKED(M)

#endif

#endif
