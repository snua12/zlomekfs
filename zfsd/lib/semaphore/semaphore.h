/* ! \file \brief Semaphore functions.  */

/* Copyright (C) 2003 Josef Zlomek

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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "system.h"
#include "pthread-wrapper.h"

// NOTE: why do not use posix semaphores?

/* ! \brief Semaphore. */
typedef struct semaphore_def
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	unsigned int value;
} semaphore;

extern int semaphore_init(semaphore * sem, unsigned int n);
extern int semaphore_destroy(semaphore * sem);
extern int semaphore_up(semaphore * sem, unsigned int n);
extern int semaphore_down(semaphore * sem, unsigned int n);

#endif
