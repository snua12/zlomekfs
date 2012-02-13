#define _GNU_SOURCE
#include <stdio.h>
#include "fh.h"
#include "zfsio.h"

typedef struct zfs_cookie_def
{
	zfs_cap cap;
	uint64_t offset;
} * zfs_cookie;


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

static ssize_t zfsfile_write(ATTRIBUTE_UNUSED void *c, ATTRIBUTE_UNUSED const char *buf, ATTRIBUTE_UNUSED size_t size)
{
//	write_res res;
//	write_args args;


//	int32_t r = zfs_write(&res, &args);
	return -1;
}

static int zfsfile_seek(ATTRIBUTE_UNUSED void *c, ATTRIBUTE_UNUSED long *offset, ATTRIBUTE_UNUSED int whence)
{
	return -1;
}

static int zfsfile_close(void *c)
{
	if (c == NULL)
		return 0;

	zfs_cookie zcookie = (zfs_cookie) c;
	zfs_close(&zcookie->cap);
	free(zcookie);

	return 0;
}

cookie_io_functions_t  zfs_io_funcs = { 
	.read  = zfsfile_read,
	.write = zfsfile_write,
	.seek  = (cookie_seek_function_t *) zfsfile_seek, // hackish typecast
	.close = zfsfile_close
}; 

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

zfs_file * zfs_fopen(zfs_fh * fh)
{
	FILE * f = fopenzfs(fh, "rb");
	return (zfs_file *) f;

}

int zfs_fclose(zfs_file * file)
{
	return fclose((FILE *) file);
}

FILE * zfs_fdget(zfs_file * file)
{
	return (FILE *) file;
}
