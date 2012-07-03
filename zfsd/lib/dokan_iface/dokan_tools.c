#include <windows.h>
#include <winbase.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "dokan_tools.h"
#include "log.h"

static const wchar_t windows_dir_delimiter[] = L"\\";
static const char unix_dir_delimiter[] = "/";

void file_path_to_dir_and_file(LPCWSTR file_path, char * dir_path, char * file_name)
{
	// make local copy of file_path
	wchar_t file_path_dup[MAX_PATH + 1];
	wcsncpy(file_path_dup, file_path, MAX_PATH);
	file_path_dup[MAX_PATH] = 0;

	char * dir_path_ptr = dir_path;

	wchar_t * ctx;
	wchar_t * tok;
	char * last_tok = NULL; // points to last part of the path 

	dir_path_ptr[0] = *unix_dir_delimiter;
	dir_path_ptr[1] = 0;

	for ( tok = wcstok(file_path_dup, windows_dir_delimiter, &ctx);
		tok != NULL;
		tok = wcstok(NULL, windows_dir_delimiter, &ctx))
	{
		size_t len = wcslen(tok);
		if (len > 0)
		{
			// add unix directory delimiter
			*dir_path_ptr = *unix_dir_delimiter;
			last_tok = dir_path_ptr;
			dir_path_ptr++;
			*dir_path_ptr = '\0';

			// append dir_path
			size_t rv = wcstombs(dir_path_ptr, tok, MAX_PATH - (dir_path_ptr - dir_path));
			if (rv > 0)
			{
				dir_path_ptr += rv;
			}

			*dir_path_ptr = '\0';
		}
	}
	
	if (file_name == NULL)
		return;

	// move last part of the path to file_name
	*file_name = 0;
	if (last_tok != NULL)
	{
		strncpy(file_name, last_tok + 1, MAX_PATH);
		// remove the last path of the file from dir_path
		// keep / when dir_path is root
		if (last_tok == dir_path)
		{
			last_tok[1] = '\0';
		}
		else
		{
			last_tok[0] = '\0';
		}
	}
}

int zfs_err_to_dokan_err(int32_t err)
{
	switch (err)
	{
		case ZFS_OK:
			return -ERROR_SUCCESS;
		case ENOENT:
			return -ERROR_FILE_NOT_FOUND;
			return -ERROR_PATH_NOT_FOUND;
		case EROFS:
			return -ERROR_WRITE_PROTECT;
		default:
			return -err;
	}
}

zfs_cap * dokan_file_info_to_cap(PDOKAN_FILE_INFO info)
{
	intptr_t context = info->Context;
	return (zfs_cap *) context;
}

void cap_to_dokan_file_info(PDOKAN_FILE_INFO info, zfs_cap * cap)
{
	intptr_t context = (intptr_t) cap;
	info->Context = context;
}

void convert_dokan_access_to_flags(uint32_t * flags,  DWORD desired_access)
{
	if ((desired_access & GENERIC_READ || desired_access & FILE_READ_DATA)
		&& (desired_access & GENERIC_WRITE || desired_access & FILE_WRITE_DATA))
	{
		*flags = O_RDWR;
		return;
	}

	if (desired_access & GENERIC_READ || desired_access & FILE_READ_DATA)
	{
		*flags = O_RDONLY;
		return;
	}

	if (desired_access & GENERIC_WRITE || desired_access & FILE_WRITE_DATA)
	{
		*flags = O_WRONLY;
		return;
	}

	// UNIX has not special execute flag
	if (desired_access & GENERIC_EXECUTE)
	{
		*flags = O_WRONLY;
	}
}

void create_args_fill_dokan_access(create_args * args, DWORD desired_access)
{

#if 0
	ctx = fuse_req_ctx(req);
	attr->mode = -1;
	attr->uid = map_uid_node2zfs(ctx->uid);
	attr->gid = map_gid_node2zfs(ctx->gid);
	attr->size = -1;
	attr->atime = -1;
	attr->mtime = -1;
#endif
	/*
	GENERIC_ALL, GENERIC_EXECUTE, GENERIC_READ, GENERIC_WRITE
	*/

	convert_dokan_access_to_flags(&args->flags, desired_access);

}


void create_args_fill_dokan_shared_mode( ATTRIBUTE_UNUSED create_args * args, ATTRIBUTE_UNUSED DWORD shared_mode)
{
	/*
	0 Prevents other processes from opening a file or device if they request delete, read, or write access.
	FILE_SHARE_DELETE Delete access allows both delete and rename operations.
	FILE_SHARE_READ
	FILE_SHARE_WRITE
	*/
}


void create_args_fill_dokan_creation_disposition(create_args * args, DWORD creation_disposition)
{
	if (creation_disposition == CREATE_ALWAYS)
	{
		args->flags |= O_CREAT;
	}

	if (creation_disposition == CREATE_NEW)
	{
		args->flags |= O_CREAT;
		args->flags |= O_TRUNC;
	}

	if (creation_disposition == OPEN_ALWAYS)
	{
		args->flags |= O_CREAT;
	}

	if (creation_disposition == OPEN_EXISTING)
	{

	}

	if (creation_disposition == TRUNCATE_EXISTING)
	{
		args->flags |= O_TRUNC;
	}
}


void create_args_fill_dokan_flags_and_attributes(ATTRIBUTE_UNUSED create_args * args, ATTRIBUTE_UNUSED DWORD flags_and_attributes)
{
	/*
	FILE_ATTRIBUTE_ARCHIVE
	FILE_ATTRIBUTE_ENCRYPTED
	FILE_ATTRIBUTE_HIDDEN
	FILE_ATTRIBUTE_NORMAL
	FILE_ATTRIBUTE_OFFLINE
	FILE_ATTRIBUTE_READONLY
	FILE_ATTRIBUTE_SYSTEM
	FILE_ATTRIBUTE_TEMPORARY

	FILE_FLAG_BACKUP_SEMANTICS
	FILE_FLAG_DELETE_ON_CLOSE
	FILE_FLAG_NO_BUFFERING
	FILE_FLAG_OPEN_NO_RECALL
	FILE_FLAG_OPEN_REPARSE_POINT
	FILE_FLAG_OVERLAPPED
	FILE_FLAG_POSIX_SEMANTICS
	FILE_FLAG_RANDOM_ACCESS
	FILE_FLAG_SEQUENTIAL_SCAN
	FILE_FLAG_WRITE_THROUGH

	SECURITY_ANONYMOUS
	SECURITY_CONTEXT_TRACKING
	SECURITY_DELEGATION
	SECURITY_EFFECTIVE_ONLY
	SECURITY_IDENTIFICATION
	SECURITY_IMPERSONATION
	*/

}

static void zfstime_to_filetime(PFILETIME ftime, zfs_time ztime)
{
	// based on microsoft kb 167296
	LONGLONG ll;
	ll = Int32x32To64(ztime, 10000000) + 116444736000000000;

	ftime->dwLowDateTime = (DWORD) ll;
	ftime->dwHighDateTime = ll >> 32;
}

void filetime_to_zfstime(zfs_time * ztime, CONST FILETIME* ftime)
{
	LONGLONG ll = ftime->dwHighDateTime;
	ll = (ll << 32) + ftime->dwLowDateTime;
	*ztime = (zfs_time)((ll - 116444736000000000) / 10000000);

}

static DWORD ftype_to_file_attrs(ftype type)
{
	switch (type)
	{
		case FT_DIR:
			return FILE_ATTRIBUTE_DIRECTORY;
		case FT_REG:
			return FILE_ATTRIBUTE_NORMAL;
		case FT_BAD:
		case FT_LNK:
		case FT_BLK:
		case FT_CHR:
		case FT_SOCK:
		case FT_FIFO:
		default:
			return FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_READONLY;
	}
}

void fattr_to_file_information(LPBY_HANDLE_FILE_INFORMATION buffer, fattr * fa)
{
	memset(buffer, 0, sizeof(*buffer));

	buffer->nFileSizeLow =  (DWORD) fa->size;
	buffer->nFileSizeHigh = (fa->size) >> 32;
	buffer->dwFileAttributes = ftype_to_file_attrs(fa->type);

	if (((fa->mode & S_IWUSR) == 0) || ((fa->mode & S_IWGRP) == 0) || ((fa->mode & S_IWOTH) == 0))
	{
		buffer->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
	}

	zfstime_to_filetime(&buffer->ftCreationTime, fa->ctime);
	zfstime_to_filetime(&buffer->ftLastAccessTime, fa->atime);
	zfstime_to_filetime(&buffer->ftLastWriteTime, fa->mtime);

	buffer->dwVolumeSerialNumber = ZFS_VOLUME_SERIAL_NUMBER;
	buffer->nNumberOfLinks = fa->nlink;
#if 0
	buffer->nFileIndexHigh;
	buffer->nFileIndexLow;

#endif

}

void fattr_to_find_dataw(PWIN32_FIND_DATAW data, fattr * fa)
{
	memset(data, 0, sizeof(*data));

	data->nFileSizeLow =  (DWORD) fa->size;
	data->nFileSizeHigh = (fa->size) >> 32;

	if ((fa->mode & S_IWUSR) || (fa->mode & S_IWGRP) || (fa->mode & S_IWOTH))
	{
		data->dwFileAttributes = 0;
	}
	else
	{
		data->dwFileAttributes = FILE_ATTRIBUTE_READONLY;
	}

	data->dwFileAttributes |= ftype_to_file_attrs(fa->type);

	zfstime_to_filetime(&data->ftCreationTime, fa->ctime);
	zfstime_to_filetime(&data->ftLastAccessTime, fa->atime);
	zfstime_to_filetime(&data->ftLastWriteTime, fa->mtime);

#if 0
	DWORD dwReserved0;
	DWORD dwReserved1;
	WCHAR  cFileName[ MAX_PATH ];
	WCHAR  cAlternateFileName[ 14 ]
#ifdef _MAC
	DWORD dwFileType;
	DWORD dwCreatorType;
	WORD  wFinderFlags;
#endif
#endif

}