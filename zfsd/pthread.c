/* Wrappers for pthread_* functions and related data.
   Copyright (C) 2004 Josef Zlomek

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

#include "system.h"
#include "pthread.h"

#ifdef ENABLE_CHECKING

/* Static mutex initializer.  */
pthread_mutex_t zfsd_mutex_initializer
  = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

#else

/* Static mutex initializer.  */
pthread_mutex_t zfsd_mutex_initializer
  = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;

#endif
