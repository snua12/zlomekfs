/* ! \file \brief Functions for threads communicating with Dokan library  */

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


#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <dokan.h>
#include <fileinfo.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#include "system.h"
#include "log.h"
#include "dir.h"
#include "file.h"
#include "thread.h"
#include "dokan_tools.h"
#include "zfs_config.h"
#include "zfs-prot.h"
#include "dokan_iface.h"

bool mounted = false;

static pthread_t dokan_thread;

DOKAN_OPTIONS zfs_dokan_options =
{
	.Version = DOKAN_VERSION,
	.ThreadCount = 0, // use default 0
	.Options = DOKAN_OPTION_KEEP_ALIVE, 
	.MountPoint = L"z:"
};

/*zlomek fs functions needs this thread specific things */
#define DOKAN_SET_THREAD_SPECIFIC \
	thread ctx = {.mutex=ZFS_MUTEX_INITIALIZER, .sem = ZFS_SEMAPHORE_INITIALIZER(0)}; \
	ctx.from_sid = this_node->id; \
	ctx.dc_call = dc_create(); \
	pthread_setspecific(thread_data_key, &ctx); \
	pthread_setspecific(thread_name_key, "Dokan worker thread"); \
	\
	lock_info li[MAX_LOCKED_FILE_HANDLES]; \
	set_lock_info(li);

#define DOKAN_CLEAN_THREAD_SPECIFIC \
	dc_destroy(ctx.dc_call);

static int32_t dokan_zfs_extended_lookup(dir_op_res * res, char *path)
{
	// shortcut for root directory
	if (strcmp(path, "/") == 0)
	{
		res->file = root_fh;
		return ZFS_OK;
	}


	char path_copy[MAX_PATH] = "";
	strcpy(path_copy, path);

	return zfs_extended_lookup(res, &root_fh, path_copy);
}

static bool_t zfs_file_exists(LPCWSTR file_name)
{

	char path[MAX_PATH] = "";
	file_path_to_dir_and_file(file_name, path, NULL);
	dir_op_res lres;
	int rv = dokan_zfs_extended_lookup(&lres, path);
	return (rv == ZFS_OK);
}

static int zfs_get_end_of_file(zfs_fh * fh, uint64_t * size)
{
	fattr fa;
	int rv = zfs_getattr(&fa, fh);
	if (rv != ZFS_OK)
	{
		return rv;
	}

	*size = fa.size;

	return ZFS_OK;
}

static int zfs_set_end_of_file(zfs_fh * fh, uint64_t size)
{
	fattr fa;
	setattr_args args;
	args.attr.size = size;
	args.attr.mode = -1;
	args.attr.uid = -1;
	args.attr.gid = -1;
	args.attr.atime = -1;
	args.attr.mtime = -1;

	//zfs_getattr(&fa, &lres.file);
	return zfs_setattr(&fa, fh, &args.attr, true);
}

static int zfs_truncate_file(zfs_fh * fh)
{
	// truncate file
	return zfs_set_end_of_file(fh, 0);
}

// for debugging purpose
#if 0
static const char * creation_disposition_to_str(DWORD creation_disposition)
{
	if (creation_disposition == CREATE_NEW)
		return "CREATE_NEW";
	if (creation_disposition == OPEN_ALWAYS)
		return "OPEN_ALWAYS";

	if (creation_disposition == CREATE_ALWAYS)
		return "CREATE_ALWAYS";

	if (creation_disposition == OPEN_EXISTING)
		return "OPEN_EXISTING";

	if (creation_disposition == TRUNCATE_EXISTING)
		return "TRUNCATE_EXISTING";

	return "";
}
#endif

// CreateFile
//   If file is a directory, CreateFile (not OpenDirectory) may be called.
//   In this case, CreateFile should return 0 when that directory can be opened.
//   You should set TRUE on DokanFileInfo->IsDirectory when file is a directory.
//   When CreationDisposition is CREATE_ALWAYS or OPEN_ALWAYS and a file already exists,
//   you should return ERROR_ALREADY_EXISTS(183) (not negative value)
static int  DOKAN_CALLBACK inner_dokan_create_file (
	LPCWSTR file_name,      	// FileName
	DWORD desired_access,        	// DesiredAccess
	DWORD shared_mode,        	// ShareMode
	DWORD creation_disposition,    // CreationDisposition
	DWORD flags_and_attributes,    // FlagsAndAttributes
	PDOKAN_FILE_INFO info)
{
	bool_t file_exists = zfs_file_exists(file_name);

	if (creation_disposition == CREATE_NEW && file_exists)
	{
		return -ERROR_FILE_EXISTS;
	}

	if (creation_disposition == TRUNCATE_EXISTING && !file_exists)
	{
		return -ERROR_FILE_NOT_FOUND;
	}


	char path[MAX_PATH] = "";
	char name[MAX_PATH] = "";

	if (creation_disposition == OPEN_EXISTING || creation_disposition == TRUNCATE_EXISTING
		|| (creation_disposition == OPEN_ALWAYS && file_exists))
	{
		file_path_to_dir_and_file(file_name, path, NULL);
	}
	else
	{
		file_path_to_dir_and_file(file_name, path, name);
	}


	char path_copy[MAX_PATH] = "";
	strcpy(path_copy, path);
	dir_op_res lres;
	int32_t rv;
	rv = dokan_zfs_extended_lookup(&lres, path_copy);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	if (creation_disposition == OPEN_EXISTING || creation_disposition == TRUNCATE_EXISTING
		|| (creation_disposition == OPEN_ALWAYS && file_exists))
	{
		if (creation_disposition == TRUNCATE_EXISTING)
		{
			// truncate file
			zfs_truncate_file(&lres.file);
		}

		zfs_cap local_cap;
		uint32_t flags = 0;
		convert_dokan_access_to_flags(&flags, desired_access);
		rv = zfs_open(&local_cap, &lres.file, flags); 
		if (rv != ZFS_OK)
		{
			return -ERROR_FILE_NOT_FOUND;
		}

		// allocate memory for new capability
		zfs_cap *cap = xmemdup(&local_cap, sizeof(*cap));
		cap_to_dokan_file_info(info, cap);

		if (creation_disposition == OPEN_ALWAYS)
		{
			return ERROR_ALREADY_EXISTS;
		}
		return -ERROR_SUCCESS;
	}

	create_args args;
	args.where.dir = lres.file;
	xmkstring(&args.where.name, name);

	args.flags = 0;
	create_args_fill_dokan_access(&args, desired_access);
	create_args_fill_dokan_shared_mode(&args, shared_mode);
	create_args_fill_dokan_flags_and_attributes(&args, flags_and_attributes);
	create_args_fill_dokan_creation_disposition(&args, creation_disposition);

	args.attr.uid = get_default_node_uid();
	args.attr.gid = get_default_node_gid();
	args.attr.mode = get_default_file_mode();

	create_res cres;
	rv = zfs_create(&cres, &args.where.dir, &args.where.name, args.flags, &args.attr); 
	xfreestring(&args.where.name);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	// set directory flag if file is directory
	info->IsDirectory = (cres.dor.attr.type == FT_DIR);
	// allocate memory for new capability
	zfs_cap *cap = xmemdup(&cres.cap, sizeof(*cap));
	cap_to_dokan_file_info(info, cap);

	return -ERROR_SUCCESS;
}

static int  DOKAN_CALLBACK zfs_dokan_create_file (
	LPCWSTR file_name,      	// FileName
	DWORD desired_access,        	// DesiredAccess
	DWORD shared_mode,        	// ShareMode
	DWORD creation_disposition,    // CreationDisposition
	DWORD flags_and_attributes,    // FlagsAndAttributes
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_create_file(
			file_name,
			desired_access,
			shared_mode,
			creation_disposition,
			flags_and_attributes,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_open_directory (
	LPCWSTR dir_name,			// FileName
	PDOKAN_FILE_INFO info)
{

	char path[MAX_PATH];
	file_path_to_dir_and_file(dir_name, path, NULL);

	int32_t rv;
	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	zfs_cap local_cap;
	rv = zfs_open(&local_cap, &lres.file, O_RDONLY); 
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	// set directory flag if file is directory
	info->IsDirectory = TRUE;
	// allocate memory for new capability
	zfs_cap *cap = xmemdup(&local_cap, sizeof(*cap));
	cap_to_dokan_file_info(info, cap);

	return -ERROR_SUCCESS;
}


static int DOKAN_CALLBACK zfs_dokan_open_directory (
	LPCWSTR dir_name,			// FileName
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_open_directory(
		dir_name,
		info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_create_directory (
	ATTRIBUTE_UNUSED LPCWSTR file_name,			// FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	char path[MAX_PATH] = "";
	char name[MAX_PATH] = "";
	file_path_to_dir_and_file(file_name, path, name);

	int rv;
	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	mkdir_args args;
	xmkstring(&args.where.name, name);

	args.where.dir = lres.file;
	args.attr.mode = get_default_directory_mode();
	args.attr.uid = get_default_node_uid();
	args.attr.gid = get_default_node_gid();

	dir_op_res res;
	rv = zfs_mkdir(&res, &args.where.dir, &args.where.name, &args.attr);
	xfreestring(&args.where.name);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	return -ERROR_SUCCESS;
}


static int DOKAN_CALLBACK zfs_dokan_create_directory (
	ATTRIBUTE_UNUSED LPCWSTR file_name,			// FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_create_directory(
			file_name,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

// When FileInfo->DeleteOnClose is true, you must delete the file in Cleanup.
static int DOKAN_CALLBACK inner_dokan_cleanup (
	ATTRIBUTE_UNUSED LPCWSTR file_name,      // FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	TRACE("");

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_cleanup (
	LPCWSTR file_name,      // FileName
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_cleanup(
			file_name,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}


static int DOKAN_CALLBACK inner_dokan_close_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,      // FileName
	PDOKAN_FILE_INFO info)
{
	char path[MAX_PATH];
	file_path_to_dir_and_file(file_name, path, NULL);

	zfs_cap * cap = dokan_file_info_to_cap(info);
	if (cap == NULL)
	{
		return -ERROR_SUCCESS;
	}

	int32_t rv = zfs_close(cap);
	free(cap);
	cap_to_dokan_file_info(info, NULL);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_close_file (
	LPCWSTR file_name,      // FileName
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_close_file(
		file_name,
		info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_read_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,  // FileName
	LPVOID buffer,   // Buffer
	DWORD number_of_bytes_to_read,    // NumberOfBytesToRead
	LPDWORD number_of_bytes_read,  // NumberOfBytesRead
	LONGLONG offset, // Offset
	PDOKAN_FILE_INFO info)
{

	if(info->IsDirectory == TRUE)
	{
		return -ERROR_INVALID_HANDLE;
	}

	zfs_cap * cap = dokan_file_info_to_cap(info);
	if (cap == NULL)
	{
		return -ERROR_INVALID_HANDLE;
	}

	*number_of_bytes_read = 0;
	while (number_of_bytes_to_read != 0)
	{
		uint32_t to_read = number_of_bytes_to_read;
		if (to_read > ZFS_MAXDATA)
		{
			to_read = ZFS_MAXDATA;
		}
		
		read_res res;
		res.data.buf = buffer + *number_of_bytes_read;
		int rv = zfs_read(&res, cap, offset + *number_of_bytes_read, to_read, true);

		char path[MAX_PATH];
		file_path_to_dir_and_file(file_name, path, NULL);

		if (rv != ZFS_OK)
		{
			return zfs_err_to_dokan_err(rv);
		}

		number_of_bytes_to_read -= res.data.len;
		*number_of_bytes_read += res.data.len;

		// end of stream
		if (res.data.len == 0)
		{
			return -ERROR_SUCCESS;
		}
	}

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_read_file (
	LPCWSTR file_name,  // FileName
	LPVOID buffer,   // Buffer
	DWORD number_of_bytes_to_read,    // NumberOfBytesToRead
	LPDWORD number_of_bytes_read,  // NumberOfBytesRead
	LONGLONG offset, // Offset
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_read_file(
			file_name,
			buffer,
			number_of_bytes_to_read,
			number_of_bytes_read,
			offset,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_write_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,  // FileName
	LPCVOID buffer,  // Buffer
	DWORD number_of_bytes_to_write,    // NumberOfBytesToWrite
	LPDWORD number_of_bytes_written,  // NumberOfBytesWritten
	LONGLONG offset, // Offset
	PDOKAN_FILE_INFO info)
{

	*number_of_bytes_written = 0;
	while (number_of_bytes_to_write > 0)
	{
		write_args args;
		args.cap = * dokan_file_info_to_cap(info);
		args.offset = offset + *number_of_bytes_written;
		args.data.buf = (char *) buffer + *number_of_bytes_written;
		args.data.len = number_of_bytes_to_write;
		// size of buffer to write is limited
		if (args.data.len > ZFS_MAXDATA)
		{
			args.data.len = ZFS_MAXDATA;
		}

		write_res res;
		int rv = zfs_write(&res, &args);
		if (rv != ZFS_OK)
		{
			return zfs_err_to_dokan_err(rv);
		}

		number_of_bytes_to_write -= res.written;
		*number_of_bytes_written += res.written;
	}

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_write_file (
	LPCWSTR file_name,  // FileName
	LPCVOID buffer,  // Buffer
	DWORD number_of_bytes_to_write,    // NumberOfBytesToWrite
	LPDWORD number_of_bytes_written,  // NumberOfBytesWritten
	LONGLONG offset, // Offset
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_write_file(
			file_name,
			buffer,
			number_of_bytes_to_write,
			number_of_bytes_written,
			offset,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_flush_file_buffers (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_flush_file_buffers (
	LPCWSTR file_name, // FileName
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_flush_file_buffers(
			file_name,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_get_file_information (
	LPCWSTR file_name,          // FileName
	LPBY_HANDLE_FILE_INFORMATION buffer, // Buffer
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	TRACE("");

	char path[MAX_PATH];
	file_path_to_dir_and_file(file_name, path, NULL);

	dir_op_res lres;
	int32_t rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return -ERROR_INVALID_FUNCTION;
	}

	fattr fa;
	rv = zfs_getattr(&fa, &lres.file);
	if (rv != ZFS_OK)
	{
		return -ERROR_INVALID_FUNCTION;
	}

	fattr_to_file_information(buffer, &fa);

	// unique file index compound from inode and volume id
	buffer->nFileIndexLow = lres.file.ino;
	buffer->nFileIndexHigh = lres.file.vid;

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_get_file_information (
	LPCWSTR file_name,          // FileName
	LPBY_HANDLE_FILE_INFORMATION buffer, // Buffer
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_get_file_information(
			file_name,
			buffer,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_find_files (
	LPCWSTR path_name,			// PathName
	PFillFindData fill_data,		// call this function with PWIN32_FIND_DATAW
	PDOKAN_FILE_INFO info)  //  (see PFillFindData definition)
{
	TRACE("");

	char path[MAX_PATH];
	file_path_to_dir_and_file(path_name, path, NULL);

	int32_t rv;

	zfs_cap * cap = dokan_file_info_to_cap(info);

	dir_entry entries[ZFS_MAX_DIR_ENTRIES];
	dir_list list;
	int32_t last_cookie = 0;
	do
	{
		list.n = 0;
		list.eof = false;
		list.buffer = entries;

		rv = zfs_readdir(&list, cap, last_cookie, ZFS_MAXDATA, &filldir_array);
		if (rv != ZFS_OK)
		{
			RETURN_INT(zfs_err_to_dokan_err(rv));
		}

		uint32_t i;
		for (i = 0; i < list.n; ++i)
		{
			dir_entry *entry = entries + i;

			last_cookie = entry->cookie;

			// don't return . or .. windows don't need them
			if (strcmp(entry->name.str, ".") == 0)
				continue;

			if (strcmp(entry->name.str, "..") == 0)
				continue;

			dir_op_res lookup_res;
			rv = zfs_extended_lookup(&lookup_res, &cap->fh, entry->name.str);
			if (rv != ZFS_OK)
			{
				continue;
			}

			 WIN32_FIND_DATAW find_data;
			 fattr_to_find_dataw(&find_data, &lookup_res.attr);
			 unix_to_windows_filename(entry->name.str, find_data.cFileName, MAX_PATH);
			 if (strlen(entry->name.str) < 14)
			 {
				 unix_to_windows_filename(entry->name.str, find_data.cAlternateFileName, 13);
			 }
			 else
			 {
				 //TODO: cAlternateFileName
			 }

			 int is_full = fill_data(&find_data, info);
			 if (is_full == 1)
			 {
				RETURN_INT(-ERROR_SUCCESS);
			 }
		}
	} while (list.eof == false);

	RETURN_INT(-ERROR_SUCCESS);
}

static int DOKAN_CALLBACK zfs_dokan_find_files (
	LPCWSTR path_name,			// PathName
	PFillFindData fill_data,		// call this function with PWIN32_FIND_DATAW
	PDOKAN_FILE_INFO info)  //  (see PFillFindData definition)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_find_files(
			path_name,
			fill_data,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_set_file_attributes (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED DWORD file_attributes,   // FileAttributes
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	//TODO: store attributes to zfs metadata
	//FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
	//FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_SYSTEM

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_set_file_attributes (
	LPCWSTR file_name, // FileName
	DWORD file_attributes,   // FileAttributes
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_set_file_attributes(
			file_name,
			file_attributes,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_set_file_time (
        LPCWSTR file_name,                // FileName
        CONST FILETIME* creation_time, // CreationTime
        CONST FILETIME* last_access_time, // LastAccessTime
        CONST FILETIME* last_write_time, // LastWriteTime
        ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	fattr fa;
	setattr_args args;
	args.attr.size =  -1;
	args.attr.mode = -1;
	args.attr.uid = -1;
	args.attr.gid = -1;
	args.attr.atime = (zfs_time) - 1;
	args.attr.mtime = (zfs_time) - 1;

	filetime_to_zfstime(&args.attr.atime, last_access_time);
	filetime_to_zfstime(&args.attr.mtime, last_write_time);

	//FIXME: ctime cannot be set by zfs_setattr
	filetime_to_zfstime(&args.attr.mtime, creation_time);

	char path[MAX_PATH] = "";
	file_path_to_dir_and_file(file_name, path, NULL);
	dir_op_res lres;
	int rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	// workaround utime command
	if (args.attr.mtime == (zfs_time) - 1 && args.attr.atime != (zfs_time) - 1)
	{
		args.attr.mtime = lres.attr.mtime;
	}

	if (args.attr.atime == (zfs_time) - 1 && args.attr.mtime != (zfs_time) -1)
	{
		args.attr.atime = lres.attr.atime;
	}

	rv = zfs_setattr(&fa, &lres.file, &args.attr, true);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_set_file_time (
        LPCWSTR file_name,                // FileName
        CONST FILETIME* creation_time, // CreationTime
        CONST FILETIME* last_access_time, // LastAccessTime
        CONST FILETIME* last_write_time, // LastWriteTime
        PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_set_file_time(
			file_name,
			creation_time,
			last_access_time,
			last_write_time,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

// You should not delete file on DeleteFile or DeleteDirectory.
// When DeleteFile or DeleteDirectory, you must check whether
// you can delete the file or not, and return 0 (when you can delete it)
// or appropriate error codes such as -ERROR_DIR_NOT_EMPTY,
// -ERROR_SHARING_VIOLATION.
// When you return 0 (ERROR_SUCCESS), you get Cleanup with
// FileInfo->DeleteOnClose set TRUE and you have to delete the
// file in Close.
static int DOKAN_CALLBACK inner_dokan_delete_file (
	LPCWSTR file_name,
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	char path[MAX_PATH] = "";
	char name[MAX_PATH] = "";
	file_path_to_dir_and_file(file_name, path, name);

	int rv;
	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	dir_op_args args;
	args.dir = lres.file;
	xmkstring(&args.name, name);
	rv = zfs_unlink(&args.dir, &args.name);
	xfreestring(&args.name);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_delete_file (
	LPCWSTR file_name,
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_delete_file(
			file_name,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_delete_directory ( 
	ATTRIBUTE_UNUSED LPCWSTR file_name,
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	char path[MAX_PATH] = "";
	char name[MAX_PATH] = "";
	file_path_to_dir_and_file(file_name, path, name);

	int rv;
	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	dir_op_args args;
	args.dir = lres.file;
	xmkstring(&args.name, name);
	rv = zfs_rmdir(&args.dir, &args.name);
	xfreestring(&args.name);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_delete_directory ( 
	LPCWSTR file_name, // FileName
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_delete_directory(
			file_name,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_move_file (
	LPCWSTR existing_file_name, // ExistingFileName
	LPCWSTR new_file_name, // NewFileName
	BOOL replace_existing,	// ReplaceExisiting
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	char existing_path[MAX_PATH] = "";
	char existing_name[MAX_PATH] = "";
	file_path_to_dir_and_file(existing_file_name, existing_path, existing_name);

	dir_op_res existing_lres;
	int rv;
 	rv = dokan_zfs_extended_lookup(&existing_lres, existing_path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	char new_path[MAX_PATH] = "";
	char new_name[MAX_PATH] = "";

	file_path_to_dir_and_file(new_file_name, new_path, new_name);

	if (replace_existing == FALSE)
	{
		if (zfs_file_exists(new_file_name))
		{
			return -ERROR_ALREADY_EXISTS;
		}
	}

	file_path_to_dir_and_file(new_file_name, new_path, new_name);

	dir_op_res new_lres;
 	rv = dokan_zfs_extended_lookup(&new_lres, new_path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}


	string s_existing_name;
	xmkstring(&s_existing_name, existing_name);
	string s_new_name;
	xmkstring(&s_new_name, new_name);
	rv = zfs_rename(&existing_lres.file, &s_existing_name, &new_lres.file, &s_new_name);

	xfreestring(&s_existing_name);
	xfreestring(&s_new_name);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_move_file (
	LPCWSTR existing_file_name, // ExistingFileName
	LPCWSTR new_file_name, // NewFileName
	BOOL replace_existing,	// ReplaceExisiting
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_move_file(
			existing_file_name,
			new_file_name,
			replace_existing,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_set_end_of_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,  // FileName
	ATTRIBUTE_UNUSED LONGLONG length, // Length
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	zfs_cap * cap = dokan_file_info_to_cap(info);
	if (cap == NULL)
	{
		return -ERROR_INVALID_HANDLE;
	}

	int rv = zfs_set_end_of_file(&(cap->fh), length);

	return zfs_err_to_dokan_err(rv);
}

static int DOKAN_CALLBACK zfs_dokan_set_end_of_file (
	LPCWSTR file_name,  // FileName
	LONGLONG length, // Length
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_set_end_of_file(
			file_name,
			length,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_set_allocation_size (
	ATTRIBUTE_UNUSED LPCWSTR file_name,  // FileName
	LONGLONG length, // Length
	PDOKAN_FILE_INFO info)
{

	zfs_cap * cap = dokan_file_info_to_cap(info);
	if (cap == NULL)
	{
		return -ERROR_INVALID_HANDLE;
	}

	uint64_t file_length;
	int rv =  zfs_get_end_of_file(&(cap->fh), &file_length);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	if (length < (LONGLONG) file_length)
	{
		rv = zfs_set_end_of_file(&(cap->fh), length);
		return zfs_err_to_dokan_err(rv);
	}

	return -ERROR_SUCCESS;
}


static int DOKAN_CALLBACK zfs_dokan_set_allocation_size (
	LPCWSTR file_name,  // FileName
	LONGLONG length, // Length
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_set_allocation_size(
			file_name,
			length,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_lock_file(
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED LONGLONG byte_offset, // ByteOffset
	ATTRIBUTE_UNUSED LONGLONG length, // Length
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_INVALID_FUNCTION;
}

static int DOKAN_CALLBACK zfs_dokan_lock_file (
	LPCWSTR file_name, // FileName
	LONGLONG byte_offset, // ByteOffset
	LONGLONG length, // Length
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_lock_file(
			file_name,
			byte_offset,
			length,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_unlock_file(
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED LONGLONG byte_offset,// ByteOffset
	ATTRIBUTE_UNUSED LONGLONG length,// Length
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	return -ERROR_INVALID_FUNCTION;
}

static int DOKAN_CALLBACK zfs_dokan_unlock_file(
	LPCWSTR file_name, // FileName
	LONGLONG byte_offset,// ByteOffset
	LONGLONG length,// Length
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_unlock_file(
			file_name,
			byte_offset,
			length,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

// see Win32 API GetVolumeInformation
static int DOKAN_CALLBACK inner_dokan_get_volume_information (
	LPWSTR volume_name_buffer, // VolumeNameBuffer
	DWORD volume_name_size,	// VolumeNameSize in num of chars
	LPDWORD volume_serial_number,// VolumeSerialNumber
	LPDWORD maximum_component_length,// MaximumComponentLength in num of chars
	LPDWORD file_system_flags,// FileSystemFlags
	LPWSTR file_system_name_buffer,	// FileSystemNameBuffer
	DWORD file_system_name_size,	// FileSystemNameSize in num of chars
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	if (zfs_config.dokan.volume_name.str == NULL)
	{
		wcsncpy(volume_name_buffer, L"ZlomekFS", volume_name_size / sizeof(WCHAR));
	}
	else
	{
		mbstowcs(volume_name_buffer, zfs_config.dokan.volume_name.str, volume_name_size / sizeof(WCHAR));
	}

	if (volume_serial_number != NULL)
	{
		*volume_serial_number = ZFS_VOLUME_SERIAL_NUMBER;
	}

	*maximum_component_length = ZFS_MAXNAMELEN;
	*file_system_flags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH; //FILE_SUPPORTS_HARD_LINKS

	if (zfs_config.dokan.file_system_name.str == NULL)
	{
		wcsncpy(file_system_name_buffer, L"ZlomekFS", file_system_name_size / sizeof(WCHAR));
	}
	else
	{
		mbstowcs(file_system_name_buffer, zfs_config.dokan.file_system_name.str, file_system_name_size / sizeof(WCHAR));
	}

	return -ERROR_SUCCESS;
}

static int DOKAN_CALLBACK zfs_dokan_get_volume_information (
	LPWSTR volume_name_buffer, // VolumeNameBuffer
	DWORD volume_name_size,	// VolumeNameSize in num of chars
	LPDWORD volume_serial_number,// VolumeSerialNumber
	LPDWORD maximum_component_length,// MaximumComponentLength in num of chars
	LPDWORD file_system_flags,// FileSystemFlags
	LPWSTR file_system_name_buffer,	// FileSystemNameBuffer
	DWORD file_system_name_size,	// FileSystemNameSize in num of chars
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_get_volume_information(
			volume_name_buffer,
			volume_name_size,
			volume_serial_number,
			maximum_component_length,
			file_system_flags,
			file_system_name_buffer,
			file_system_name_size,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK zfs_dokan_unmount (
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_SUCCESS;
}

// Suported since 0.6.0. You must specify the version at DOKAN_OPTIONS.Version.
static int DOKAN_CALLBACK inner_dokan_get_file_security (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED PSECURITY_INFORMATION security_information, // A pointer to SECURITY_INFORMATION value being requested
	ATTRIBUTE_UNUSED PSECURITY_DESCRIPTOR security_descriptor, // A pointer to SECURITY_DESCRIPTOR buffer to be filled
	ATTRIBUTE_UNUSED ULONG security_descriptor_length, // length of Security descriptor buffer
	ATTRIBUTE_UNUSED PULONG length_needed, // LengthNeeded
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	return -ERROR_INVALID_FUNCTION;
}

static int DOKAN_CALLBACK zfs_dokan_get_file_security (
	LPCWSTR file_name, // FileName
	PSECURITY_INFORMATION security_information, // A pointer to SECURITY_INFORMATION value being requested
	PSECURITY_DESCRIPTOR security_descriptor, // A pointer to SECURITY_DESCRIPTOR buffer to be filled
	ULONG security_descriptor_length, // length of Security descriptor buffer
	PULONG length_needed, // LengthNeeded
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_get_file_security(
			file_name,
			security_information,
			security_descriptor,
			security_descriptor_length,
			length_needed,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

static int DOKAN_CALLBACK inner_dokan_set_file_security (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED PSECURITY_INFORMATION secrity_information,
	ATTRIBUTE_UNUSED PSECURITY_DESCRIPTOR security_descriptor, // SecurityDescriptor
	ATTRIBUTE_UNUSED ULONG security_descriptor_length, // SecurityDescriptor length
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	return -ERROR_INVALID_FUNCTION;
}

static int DOKAN_CALLBACK zfs_dokan_set_file_security (
	LPCWSTR file_name, // FileName
	PSECURITY_INFORMATION security_information,
	PSECURITY_DESCRIPTOR security_descriptor, // SecurityDescriptor
	ULONG security_descriptor_length, // SecurityDescriptor length
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC
	int rv = inner_dokan_set_file_security(
			file_name,
			security_information,
			security_descriptor,
			security_descriptor_length,
			info);
	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

/* dokan filesystem interface */
DOKAN_OPERATIONS zfs_dokan_operations =
{
	.CreateFile = zfs_dokan_create_file,
	.OpenDirectory = zfs_dokan_open_directory,
	.CreateDirectory = zfs_dokan_create_directory,
	.Cleanup = zfs_dokan_cleanup,
	.CloseFile = zfs_dokan_close_file,
	.ReadFile = zfs_dokan_read_file,
	.WriteFile = zfs_dokan_write_file,
	.FlushFileBuffers = zfs_dokan_flush_file_buffers,
	.GetFileInformation = zfs_dokan_get_file_information,
	.FindFiles = zfs_dokan_find_files,
	.FindFilesWithPattern = NULL,
	.SetFileAttributes = zfs_dokan_set_file_attributes,
	.SetFileTime = zfs_dokan_set_file_time,
	.DeleteFile = zfs_dokan_delete_file,
	.DeleteDirectory = zfs_dokan_delete_directory,
	.MoveFile = zfs_dokan_move_file,
	.SetEndOfFile = zfs_dokan_set_end_of_file,
	.SetAllocationSize = zfs_dokan_set_allocation_size,
	.LockFile = zfs_dokan_lock_file,
	.UnlockFile = zfs_dokan_unlock_file,
	.GetFileSecurity = zfs_dokan_get_file_security,
	.SetFileSecurity = zfs_dokan_set_file_security,
	.GetDiskFreeSpace = NULL,
	.GetVolumeInformation = zfs_dokan_get_volume_information,
	.Unmount = zfs_dokan_unmount
};

static void * dokan_main(ATTRIBUTE_UNUSED void * data)
{
	thread_disable_signals();

	lock_info li[MAX_LOCKED_FILE_HANDLES];
	set_lock_info(li);

	// pass zlomek options to DOKAN OPTIONS
	wchar_t wMountPoint[MAX_PATH + 1];
	mbstowcs(wMountPoint, zfs_config.mountpoint, MAX_PATH);
	zfs_dokan_options.MountPoint = wMountPoint;
	size_t thread_count = zfs_config.threads.kernel_thread_limit.max_total;
	if (thread_count > 0)
	{
		zfs_dokan_options.ThreadCount = thread_count; 
	}

	// for debuging purpose
	zfs_dokan_options.ThreadCount = 1; 

	mounted = true;
	int status = DokanMain(&zfs_dokan_options, &zfs_dokan_operations);
        switch (status) {
        case DOKAN_SUCCESS:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Success\n", __func__);
                break;
        case DOKAN_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Error\n", __func__);
                break;
        case DOKAN_DRIVE_LETTER_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Bad Drive letter\n", __func__);
                break;
        case DOKAN_DRIVER_INSTALL_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Can't install driver\n", __func__);
                break;
        case DOKAN_START_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Driver something wrong\n", __func__);
                break;
        case DOKAN_MOUNT_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Can't assign a drive letter\n", __func__);
                break;
        case DOKAN_MOUNT_POINT_ERROR:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Mount point error\n", __func__);
                break;
        default:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Unknown error: %d\n", __func__, status);
                break;
        }

	mounted = false;

	/* notify zfsd daemon about fuse thread termination */
	pid_t pid = getpid();
	kill(pid, SIGTERM);

	return NULL;
}

bool kernel_start(void)
{
	int rv = pthread_create(&dokan_thread, NULL, dokan_main, NULL);
	return (rv != -1);
}

void kernel_unmount(void)
{
	if (mounted)
	{
		DokanUnmount(zfs_dokan_options.MountPoint[0]);
	}
}

void kernel_cleanup(void)
{
	// nothing to do there
}

