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

#include "system.h"
#include <unistd.h>
#include "constant.h"

/* Maximal number of file descriptors.  */
int max_nfd;

/* Maximal number of network sockets.  */
int max_network_sockets;

/* Maximal number of file descriptors for local files.  */
int max_local_fds;

/* Maximal number of file descriptors for files containing intervals.  */
int max_interval_fds;

/* Initialize the constants */

void
init_constants ()
{
  max_nfd = getdtablesize ();
  max_network_sockets = max_nfd / 4;
  max_local_fds = max_nfd / 4;
  max_interval_fds = max_nfd / 4;
}
