/* ! \file \brief Helper functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

#include "system.h"
#include "util.h"
#include "log.h"
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pthread-wrapper.h"

/* ! Print LEN bytes of buffer BUF to file F in hexadecimal ciphers. !see
   message */
void
print_hex_buffer(int level, ATTRIBUTE_UNUSED FILE * f, char *buf,
				 unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++)
	{
		if (i > 0)
		{
			if (i % 16 == 0)
				message(level, FACILITY_DATA, "\n");
			else if (i % 4 == 0)
				message(level, FACILITY_DATA, " ");
		}
		message(level, FACILITY_DATA, "%02x ", (unsigned char)buf[i]);
	}
	message(level, FACILITY_DATA, "\n");
}

/* ! Read LEN bytes from file descriptor FD to buffer BUF.  */

bool full_read(int fd, void *buf, size_t len)
{
	ssize_t r;
	unsigned int total_read;

	for (total_read = 0; total_read < len; total_read += r)
	{
again:
		r = read(fd, (char *)buf + total_read, len - total_read);
		if (r <= 0)
		{
			if (errno == EINTR)
			{
				goto again;
			}

			message(LOG_WARNING, FACILITY_DATA,
					"reading data FAILED: %d (%s)\n", errno, strerror(errno));
			return false;
		}
	}

	message(LOG_DEBUG, FACILITY_DATA,
			"Reading data of length %u from %d to %p:\n", len, fd, buf);
	print_hex_buffer(LOG_DATA, NULL, (char *)buf, len);

	return true;
}

/* ! Write LEN bytes from buffer BUF to file descriptor FD.  */

bool full_write(int fd, void *buf, size_t len)
{
	ssize_t w;
	unsigned int total_written;

	message(LOG_DEBUG, FACILITY_DATA,
			"Writing data of length %u to %d from %p:\n", len, fd, buf);
	print_hex_buffer(LOG_DATA, NULL, (char *)buf, len);

	for (total_written = 0; total_written < len; total_written += w)
	{
again:
		w = write(fd, (char *)buf + total_written, len - total_written);
		if (w <= 0)
		{
			if (errno == EINTR)
			{
				goto again;
			}

			message(LOG_NOTICE, FACILITY_DATA,
					"writing data FAILED: %d (%s)\n", errno, strerror(errno));
			return false;
		}
	}

	return true;
}

/* ! Create a full path PATH with access rights MODE (similarly as "mkdir -p
   path" does).  Return true if PATH exists at the end of this function.  */

bool full_mkdir(char *path, unsigned int mode)
{
	struct stat st;
	char *last;
	char *end;

	if (lstat(path, &st) == 0)
	{
		return (st.st_mode & S_IFMT) == S_IFDIR;
	}
	else
	{
		if (mkdir(path, mode) == 0)
			return true;

		if (errno != ENOENT)
			return false;

		for (last = path; *last; last++)
			;
		last--;

		/* Find the first component of PATH which can be created.  */
		while (1)
		{
			for (end = last; end != path && *end != '/'; end--)
				;
			if (end == path)
				return false;

			*end = 0;

			if (mkdir(path, mode) == 0)
				break;

			if (errno != ENOENT)
				return false;
		}

		/* Create the rest of components.  */
		while (1)
		{
			*end = '/';

			if (mkdir(path, mode) != 0)
				return false;

			for (end++; end < last && *end; end++)
				;
			if (end >= last)
				return true;
		}
	}

	return false;
}

/* ! Return true if all LEN bytes of buffer P are equal to BYTE.  */

bool bytecmp(const void *p, int byte, size_t len)
{
	const unsigned char *s;

	byte = (unsigned int)byte & 255;
	for (s = (const unsigned char *)p; len-- > 0; s++)
		if (*s != byte)
			return false;

	return true;
}
