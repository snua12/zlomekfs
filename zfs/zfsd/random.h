/*! \file
    \brief Random bytes.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef RANDOM_H
#define RANDOM_H

/*! Return a pseudo-random number from range 0..NUM-1 */
#define RANDOM(NUM) (random () % NUM)

/*! File descriptor for /dev/random.  */
extern int fd_random;

/*! File descriptor for /dev/urandom.  */
extern int fd_urandom;

extern bool initialize_random_c (void);
extern void cleanup_random_c (void);

#endif
