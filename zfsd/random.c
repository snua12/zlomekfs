/* Random bytes.
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

#include "system.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "random.h"

/* File descriptor for /dev/random.  */
int fd_random = -1;

/* File descriptor for /dev/urandom.  */
int fd_urandom = -1;

/* Initialize random bytes.  */

bool
initialize_random_c (void)
{
  fd_random = open ("/dev/random", O_RDONLY);
  if (fd_random < 0)
    return false;

  fd_urandom = open ("/dev/urandom", O_RDONLY);
  if (fd_urandom < 0)
    return false;

  return true;
}

/* Close files opened in init_random_c.  */

void
cleanup_random_c (void)
{
  if (fd_random >= 0)
    close (fd_random);
  if (fd_urandom >= 0)
    close (fd_urandom);
}
