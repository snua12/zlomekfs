/*! Semaphore functions.
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

#include "system.h"
#include "pthread.h"
#include "semaphore.h"

/*! Initialize semaphore SEM and set its value to N.  */

int
semaphore_init (semaphore *sem, unsigned int n)
{
  int r;

  r = zfsd_mutex_init (&sem->mutex);
  if (r != 0)
    return r;

  r = zfsd_cond_init (&sem->cond);
  if (r != 0)
    {
      zfsd_mutex_destroy (&sem->mutex);
      return r;
    }

  sem->value = n;
  return 0;
}

/*! Destroy semaphore SEM.  */

int
semaphore_destroy (semaphore *sem)
{
  int r;

  r = zfsd_cond_destroy (&sem->cond);
  if (r != 0)
    return r;

  r = zfsd_mutex_destroy (&sem->mutex);
  return r;
}

/*! Increase semaphore SEM by N.  */

int
semaphore_up (semaphore *sem, unsigned int n)
{
  zfsd_mutex_lock (&sem->mutex);
  sem->value += n;
  zfsd_cond_signal (&sem->cond);
  zfsd_mutex_unlock (&sem->mutex);

  return 0;
}

/*! Decrease semaphore SEM by N.  */

int
semaphore_down (semaphore *sem, unsigned int n)
{
  zfsd_mutex_lock (&sem->mutex);
  while (sem->value < n)
    zfsd_cond_wait (&sem->cond, &sem->mutex);
  sem->value -= n;
  zfsd_mutex_unlock (&sem->mutex);

  return 0;
}
