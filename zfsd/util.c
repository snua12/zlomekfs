/* Helper functions.
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
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include "log.h"

/* Read LEN bytes from file descriptor FD to buffer BUF.  */

bool
full_read (int fd, char *buf, size_t len)
{
  ssize_t r;
  unsigned int total_read;

  if (verbose >= 2)
    {
      size_t i;

      message (2, stderr, "Reading data from %d to %p:\n", fd, buf);
      for (i = 0; i < len; i++)
	fprintf (stderr, "%02x ", (unsigned char) buf[i]);
      fprintf (stderr, "\n");
    }

  for (total_read = 0; total_read < len; total_read += r)
    {
      r = read (fd, buf + total_read, len - total_read);
      if (r <= 0)
	{
	  message (2, stderr, "reading data FAILED\n");
	  return false;
	}
    }

  message (2, stderr, "reading data SUCCEDED\n");
  return true;
}

/* Write LEN bytes from buffer BUF to file descriptor FD.  */

bool
full_write (int fd, char *buf, size_t len)
{
  ssize_t w;
  unsigned int total_written;

  if (verbose >= 2)
    {
      size_t i;

      message (2, stderr, "Writing data to %d from %p:\n", fd, buf);
      for (i = 0; i < len; i++)
	fprintf (stderr, "%02x ", (unsigned char) buf[i]);
      fprintf (stderr, "\n");
    }

  for (total_written = 0; total_written < len; total_written += w)
    {
      w = write (fd, buf + total_written, len - total_written);
      if (w <= 0)
	{
	  message (2, stderr, "writing data FAILED\n");
	  return false;
	}
    }

  message (2, stderr, "writing data SUCCEDED\n");
  return true;
}
