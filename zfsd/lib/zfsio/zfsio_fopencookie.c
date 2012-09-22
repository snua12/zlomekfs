/*! 
 *  \file zfsio_fopencookie.c
 *  \brief Emulates stdio file api on zlomekFS file api.
 *  \author Ales Snuparek
 *
 *  Implements readonly support for stdio file api, which use FILE * on
 *  zlomekfs api which use zfs_fh * as file destciptor.
 *  This wrapper is implemented by fopencookie function,
 *  which is GNU extension.
 */

/* Copyright (C) 2008, 2012 Ales Snuparek

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

#define _GNU_SOURCE
#include <stdio.h>
#include "fh.h"
#include "zfsio.h"

/*! \brief for keeping information about opened file */
typedef struct zfs_cookie_def
{
	zfs_cap cap;
	uint64_t offset;
} * zfs_cookie;

/*! \brief implements fread */
static ssize_t zfsfile_read(void *c, char *buf, size_t size)
{
	zfs_cookie zcookie = (zfs_cookie) c;

	read_res res;
	res.data.buf = buf;
	int32_t r = zfs_read(&res, &zcookie->cap, zcookie->offset, size, true);
	if (r != ZFS_OK)
		return -1;

	zcookie->offset += res.data.len;

	return res.data.len;
}

/*! \brief should implement fwrite, but implementation is missing */
static ssize_t zfsfile_write(ATTRIBUTE_UNUSED void *c, ATTRIBUTE_UNUSED const char *buf, ATTRIBUTE_UNUSED size_t size)
{
	// only read support
	return -1;
}

/*! \brief should implement fseek, but implementation is missing */
static int zfsfile_seek(ATTRIBUTE_UNUSED void *c, ATTRIBUTE_UNUSED long *offset, ATTRIBUTE_UNUSED int whence)
{
	// no seek is need by libconfig
	return -1;
}

/*! \brief implements fclose */
static int zfsfile_close(void *c)
{
	if (c == NULL)
		return 0;

	zfs_cookie zcookie = (zfs_cookie) c;
	zfs_close(&zcookie->cap);
	free(zcookie);

	return 0;
}

/*! \brief stdio file io function implementation assignment */
cookie_io_functions_t  zfs_io_funcs = { 
	.read  = zfsfile_read,
	.write = zfsfile_write,
	.seek  = (cookie_seek_function_t *) zfsfile_seek, // hackish typecast
	.close = zfsfile_close
}; 

/*! like fdopen but for zfs_fh type */
static FILE * fopenzfs(zfs_fh * fh, const char * mode)
{
	zfs_cookie zcookie = xmalloc(sizeof(struct zfs_cookie_def));
	int32_t r = zfs_open(&zcookie->cap, fh, O_RDONLY);
	if (r != ZFS_OK)
	{
		message(LOG_ERROR, FACILITY_CONFIG, ": open(): %s\n", 
				zfs_strerror(r));
		free(zcookie);
		return NULL;
	}

	zcookie->offset = 0;

	FILE * f = fopencookie(zcookie, mode, zfs_io_funcs);
	if (f == NULL)
	{
		zfsfile_close(zcookie);
		return NULL;
	}
	
	return f;
}

/*! \brief fopen wrapper */
zfs_file * zfs_fopen(zfs_fh * fh)
{
	FILE * f = fopenzfs(fh, "rb");
	return (zfs_file *) f;

}

/*! \brief fclose wrapper */
int zfs_fclose(zfs_file * file)
{
	return fclose((FILE *) file);
}

/*! \brief converts zfs_file * to FILE */
FILE * zfs_fdget(zfs_file * file)
{
	return (FILE *) file;
}
