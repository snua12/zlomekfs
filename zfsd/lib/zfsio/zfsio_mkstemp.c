#include "zfsio.h"
#include "log.h"

#define ZFS_TMP_SHARED_CONFIG_TEMPLATE "/tmp/.zfs_shared_configXXXXXXX"

struct zfs_file_def
{
	FILE * stream;
	char tmp_file[sizeof(ZFS_TMP_SHARED_CONFIG_TEMPLATE)];
};

static bool zfs_read_to_local_file(zfs_fh * fh, FILE * stream)
{
	zfs_cap cap;
	int32_t r;
	r = zfs_open(&cap, fh, O_RDONLY);
	if (r != ZFS_OK)
	{
		message(LOG_ERROR, FACILITY_CONFIG, ": open(): %s\n", 
				zfs_strerror(r));
		return false;
	}

	uint64_t offset = 0;

	char buf[ZFS_MAXDATA];

	read_res res;

	while (true)
	{
		res.data.buf = buf;

		r = zfs_read(&res, &cap, offset, ZFS_MAXDATA, true);
		if (r != ZFS_OK)
		{
			message(LOG_ERROR, FACILITY_CONFIG, ": read(): %s\n", 
					zfs_strerror(r));
			zfs_close(&cap);
			return false;
		}

		if (res.data.len == 0)
			break;

		offset += res.data.len;

		size_t written = fwrite(buf, sizeof(char), res.data.len, stream);
		if (written != res.data.len)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to write config to temporary file\n");
			zfs_close(&cap);
			return false;
		}
	}

	r = zfs_close(&cap);
	if (r != ZFS_OK)
	{
		message(LOG_ERROR, FACILITY_CONFIG, ": close(): %s\n", //file_name,
				zfs_strerror(r));
		return false;
	}

	return true;
}

zfs_file * zfs_fopen(zfs_fh * fh)
{
	zfs_file * file = xmalloc(sizeof(zfs_file));
	strncpy(file->tmp_file, ZFS_TMP_SHARED_CONFIG_TEMPLATE, sizeof(ZFS_TMP_SHARED_CONFIG_TEMPLATE));

	int fd = mkstemp(file->tmp_file);
	if (fd == -1)
	{
		free(file);
		message(LOG_ERROR, FACILITY_CONFIG, "mkstemp() has failed\n");
		return NULL;
	}

	FILE * stream = fdopen(fd, "r+");
	if (stream == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "fdopen() has failed\n");
		free(file);
		close(fd);
		return NULL;
	}

	bool ret = zfs_read_to_local_file(fh, stream);
	if (ret != true)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to copy remote file to tmp local one.\n");
		free(file);
		fclose(stream);
		return NULL;
	}


	//rewind file
	int rv = fseek(stream, 0L, SEEK_SET);
	if (rv == -1)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to fseek local tmp file to tmp local one.\n");
		free(file);
		fclose(stream);
		return NULL;
	}

	file->stream = stream;

	return file;
}

int zfs_fclose(zfs_file * file)
{
	int rv = fclose(file->stream);
	unlink(file->tmp_file);
	free(file);
	return rv;
}

FILE * zfs_fdget(zfs_file * file)
{
	return file->stream;
}

