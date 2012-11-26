/*!
 *  \file http_iface.c 
 *  \brief Interface between zloemkFS and libmicrohttpd
 *  \author Ales Snuparek
 *
 *  This library implements interface between zlomekFS and libmicrohttpd.
 *  This is experimental implementation. Do not use it on release build.
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

#include <sys/types.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include "thread.h"

#include <microhttpd.h>
#include "dir.h"
#include "file.h"

#include "fs-iface.h"

static struct MHD_Daemon * d = NULL;
#define mounted (d != NULL)

#define PAGE "<html><head><title>libmicrohttpd demo</title>"\
             "</head><body>libmicrohttpd demo</body></html>"

#define PAGE_FOUND "<html><head><title>File was found</title>"\
			"</head><body>FOUND</body></html>"

#define PAGE_NOT_FOUND "<html><head><title>File was not found</title>"\
			"</head><body>NOT FOUND</body></html>"

#define DEFAULT_PAGE_SIZE   (3 * 1024)

/// file reader for MHD_create_zfs_response_from_file
static int MHD_zfs_read(void * cls, uint64_t pos, char * buf, int max)
{
        zfs_cap * cap = (zfs_cap *) cls;
	read_res res;
	res.data.buf = buf;
	int rv = zfs_read(&res, cap, pos, max, true);
	if (rv != ZFS_OK)
	{
		return -1;
	}

	if (res.data.len == 0)
		return -1;
	
	return res.data.len;
}

/// file open for MHD_create_zfs_response_from_file
static zfs_cap * MHD_zfs_open(zfs_fh * fh, uint32_t flags)
{

	zfs_cap * local_cap = xmalloc(sizeof(zfs_cap));
	int rv = zfs_open(local_cap, fh, flags); 
	if (rv != ZFS_OK)
	{
		xfree(local_cap);
		return NULL;
	}

	return local_cap;
}

/// file close for MHD_create_zfs_response_from_file
static void MHD_zfs_close(void * cls)
{
	zfs_cap * cap = (zfs_cap *) cls;
	zfs_close(cap);
	xfree(cap);
}

// returns file size
static uint64_t MHD_zfs_get_size(zfs_fh * fh)
{
	fattr fa;
	int rv =  zfs_getattr(&fa, fh);
	if (rv != ZFS_OK)
	{
		return 0;
	}

	return fa.size;
}

static struct MHD_Response * MHD_zfs_create_response_from_file(zfs_fh * fh)
{
	// get file size

	uint64_t size = MHD_zfs_get_size(fh);
	zfs_cap * local_cap = MHD_zfs_open(fh, O_RDONLY);
	if (local_cap == NULL)
	{
		return NULL;
	}

	return  MHD_create_response_from_callback(size, DEFAULT_PAGE_SIZE,
						MHD_zfs_read, local_cap,
						MHD_zfs_close);
}

static void print_zfs_file_entry(zfs_fh * fh, const char * url, dir_entry * entry, char * buf)
{
	if (strcmp(entry->name.str, ".") == 0) return;

	dir_op_res lookup_res;
	int rv = zfs_extended_lookup(&lookup_res, fh, entry->name.str);
	if (rv != ZFS_OK)
	{
		return;
	}


	char time_str[1024];
	time_t t = lookup_res.attr.ctime;
	ctime_r(&t, time_str);

	char link_str[256];

	if (ZFS_FH_EQ(*fh, root_fh))
	{
		if (strcmp(entry->name.str, "..") == 0)
		{
			return;
		}
		
		sprintf(link_str, "/%s", entry->name.str);
	}
	else
	{

		char url_copy[1024];
		strcpy(url_copy, url);
		size_t url_len = strlen(url_copy);
		if (url_len > 1 && url_copy[url_len - 1] == '/')
		{
			url_copy[url_len - 1] = 0;
		}

		if (strcmp(entry->name.str, "..") == 0)
		{
			char * pos = strrchr(url_copy, '/');
			if (pos != NULL)
			{
				*pos = 0;
			}
		}

		sprintf(link_str, "%s/%s", url_copy, entry->name.str);
	}


		sprintf(buf,
			"<tr><td><a href=\"%s\">%s</a></td><td align=\"right\">%s</td>"
			"<td align=\"right\"> %lld</td></tr>\n",
			link_str, entry->name.str, time_str, lookup_res.attr.size);
}

static struct MHD_Response * MHD_zfs_create_response_from_dir(zfs_fh * fh, const char * url)
{
	char buf[10000] = "<html><head><title>Dir List</title><body><table>"
			"<tr><td>name</a></td><td>ctime</td>"
			"<td align=\"right\">size</td></tr>";
	zfs_cap local_cap;
	int rv = zfs_open(&local_cap, fh, O_RDONLY); 
	if (rv != ZFS_OK)
	{
		return NULL;
	}

	dir_entry entries[ZFS_MAX_DIR_ENTRIES];
	dir_list list;
	int32_t last_cookie = 0;
	do
	{
		list.n = 0;
		list.eof = false;
		list.buffer = entries;


		rv = zfs_readdir(&list, &local_cap, last_cookie, ZFS_MAXDATA, &filldir_array);
		if (rv != ZFS_OK)
		{
			return NULL;
		}

		uint32_t i;
		for (i = 0; i < list.n; ++i)
		{
			dir_entry *entry = entries + i;
			last_cookie = entry->cookie;
			print_zfs_file_entry(&local_cap.fh, url, entry, buf + strlen(buf));
		}
	}
	while (list.eof == false);

	zfs_close(&local_cap);


	strcat(buf, "</table></body></html>");

	return MHD_create_response_from_data(strlen(buf),
		(void*) buf,
		MHD_NO,
		MHD_NO);
}

static int ahc_echo(ATTRIBUTE_UNUSED void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
                    ATTRIBUTE_UNUSED const char * version,
		    ATTRIBUTE_UNUSED const char * upload_data,
		    size_t * upload_data_size,
                    void ** ptr)
{
	static int dummy;
	struct MHD_Response * response;
	int ret;

		
	lock_info li[MAX_LOCKED_FILE_HANDLES]; 
	set_lock_info(li);


	if (0 != strcmp(method, "GET"))
		return MHD_NO; /* unexpected method */

	if (&dummy != *ptr) 
	{
		/* The first time only the headers are valid,
		do not respond in the first round... */
		*ptr = &dummy;
		return MHD_YES;
	}

	if (0 != *upload_data_size)
		return MHD_NO; /* upload data in a GET!? */

	*ptr = NULL; /* clear context pointer */


	thread ctx = {.mutex=ZFS_MUTEX_INITIALIZER, .sem = ZFS_SEMAPHORE_INITIALIZER(0)}; 
	ctx.from_sid = this_node->id; 
	ctx.dc_call = dc_create(); 
	pthread_setspecific(thread_data_key, &ctx); 
	pthread_setspecific(thread_name_key, "Httpd worker thread"); 

	if (strcmp(url, "") == 0 || strcmp(url, "/") == 0)
	{
		response = MHD_zfs_create_response_from_dir(&root_fh, "/");
	}
	else
	{
		dir_op_res lres;
		char * url_copy = strdup(url);
		ret = zfs_extended_lookup(&lres, &root_fh, url_copy);
		free(url_copy);
		if (ret == ZFS_OK)
		{
			switch (lres.attr.type)
			{
				case FT_DIR:
					response = MHD_zfs_create_response_from_dir(&lres.file, url);
					break;
				case FT_REG:
					response = MHD_zfs_create_response_from_file(&lres.file);
					break;
				default:
					response = MHD_create_response_from_data(strlen(PAGE_FOUND),
							(void*) PAGE_FOUND,
							MHD_NO,
							MHD_NO);
					break;
			}
		}
		else
		{
			response = MHD_create_response_from_data(strlen(PAGE_NOT_FOUND),
					(void*) PAGE_NOT_FOUND,
					MHD_NO,
					MHD_NO);
		}
		dc_destroy(ctx.dc_call);
	}

	ret = MHD_queue_response(connection,
		MHD_HTTP_OK,
		response);

	MHD_destroy_response(response);
	return ret;
}

/*! \brief export filesystem to OS
 *
 *  Part of fs-iface implementation, export filesystem to OS.
 *  \return true on success
 *  \return false in case of error
 */
bool http_fs_start(void)
{
	d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		8080,
		NULL,
		NULL,
		&ahc_echo,
		PAGE,
		MHD_OPTION_END);

	return (d != NULL);
}

/*! \brief disconnect filesystem from exported volumes
 *
 *  Part of fs-iface implementation, disconnect filesystem from exported volumes.
 */
void http_fs_unmount(void)
{
	if (d != NULL)
	{
		MHD_stop_daemon(d);
		d = NULL;
	}
}

/*! \brief cleanup http-iface internal structures
 *
 *  Part of fs-iface implementation, cleanup internal data structures in http-iface.
 */
void http_fs_cleanup(void)
{
	// nothing to do there
}

