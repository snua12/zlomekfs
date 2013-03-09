/*!
 *  \file dokan_iface.c 
 *  \brief Interface implementation between zloemkFS and Dokan library
 *  \author Ales Snuparek
 *
 *  This library implements interface between zlomekFS and Dokan library.
 *  Dokan library interface is based on win32api. ZlomekFS file api is
 *  based on POSIX.
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

#define DOKAN_SINGLE_THREAD // run dokan interface in single thread

#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dokan.h>
#include <fileinfo.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <libgen.h>

#include "system.h"
#include "log.h"
#include "dir.h"
#include "file.h"
#include "thread.h"
#include "dokan_tools.h"
#include "zfs_config.h"
#include "zfs-prot.h"
#include "dokan_iface.h"
#include "fs-iface.h"

/*! \brief if filesystem exported to OS? */
bool mounted = false;

/*! \brief thread id of Dokan interface thread */
static pthread_t dokan_thread;

static wchar_t dokan_mount_point[ZFS_MAXNAMELEN + 1] = L"z:";

/*! \brief dokan option structure with zlomekFS default setting */
DOKAN_OPTIONS zfs_dokan_options =
{
	.Version = DOKAN_VERSION,
	.ThreadCount = 0, // use default 0
	.Options = DOKAN_OPTION_KEEP_ALIVE | DOKAN_OPTION_REMOVABLE, 
	.MountPoint = dokan_mount_point,
};

/*! \brief sets thread specific values required by other parts of zlomekFS code
 * 
 *  ZlomekFS code requires some TLS variables (thread ctx and lock_info). 
 *  This macro initialize them. 
 *  Every inner_dokan_* function in this file is encapsulated
 *  by \ref DOKAN_SET_THREAD_SPECIFIC and \ref DOKAN_CLEAN_THREAD_SPECIFIC macro.
 */
#define DOKAN_SET_THREAD_SPECIFIC \
	thread ctx = {.mutex=ZFS_MUTEX_INITIALIZER, .sem = ZFS_SEMAPHORE_INITIALIZER(0)}; \
	ctx.from_sid = this_node->id; \
	ctx.dc_call = dc_create(); \
	pthread_setspecific(thread_data_key, &ctx); \
	pthread_setspecific(thread_name_key, "Dokan worker thread"); \
	\
	lock_info * li = xmalloc(sizeof(lock_info) * MAX_LOCKED_FILE_HANDLES); \
	set_lock_info(li); 

/*! \brief cleanup thread specific values required by other parts of zlomekFS code
 *
 * This macro cleanup TLS variables (thread ctx nad lock_info).
 */
#define DOKAN_CLEAN_THREAD_SPECIFIC \
	dc_destroy(ctx.dc_call); \
	xfree(li);

#define DOKAN_CHECK_PATH_LIMIT(a) ((a) < ZFS_MAXPATHLEN)

#define DOKAN_CONVERT_PATH_TO_UNIX_BEGIN \
	size_t path_len = wcslen(file_name); \
	if (!DOKAN_CHECK_PATH_LIMIT(path_len)) return zfs_err_to_dokan_err(ENAMETOOLONG);\
	char * unix_path = xmalloc(sizeof(char) * (ZFS_MAXPATHLEN + 1));\
	rv = windows_to_unix_path(file_name, unix_path, ZFS_MAXPATHLEN + 1); \
	if (rv != ZFS_OK) rv = zfs_err_to_dokan_err(rv);



#define DOKAN_CONVERT_PATH_TO_UNIX_END \
	if (unix_path != NULL) xfree(unix_path);

/*! \brief find for given \p path filehandle
 *
 *  \param res is a dir_op_res
 *  \param path is a string
 *  \return ZFS_* 
 */
static int32_t dokan_zfs_extended_lookup(dir_op_res * res, char *path)
{
	// shortcut for root directory
	if (strcmp(path, "/") == 0)
	{
		res->file = root_fh;
		return ZFS_OK;
	}


	if (strlen(path) > ZFS_MAXPATHLEN) return ENAMETOOLONG;
	char * path_copy = xstrdup(path);
	int32_t rv = zfs_extended_lookup(res, &root_fh, path_copy);
	xfree(path_copy);
	return rv;
}

/*! \brief check if given \p file_name exists on zlomekFS volume
 *
 *  \param file_name is a LPCWSTR
 *  \return true file exitst
 *  \return ZFS_OK or ..
 */

static int32_t zfs_file_exists(char * file_name)
{
	dir_op_res lres;
	return dokan_zfs_extended_lookup(&lres, file_name);
}

/*! \brief get the file size
 *
 *  \param fh handle to the file
 *  \param size pointer to uint64_t where the file's size is stored
 *  \return ZFS_OK
 */
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

static int zfs_get_file_type(const char * path)
{
	int rv;
	dir_op_res lres;
	char * path_dup = xstrdup(path);
	rv = dokan_zfs_extended_lookup(&lres, path_dup);
	xfree(path_dup);
	if (rv != ZFS_OK)
	{
		return FT_BAD;
	}

	fattr fa;
	rv = zfs_getattr(&fa, &lres.file);
	if (rv != ZFS_OK)
	{
		return FT_BAD;
	}

	return fa.type;
}

/*! \brief set the file size
 *
 *  \param fh handle to file
 *  \param size is the new size of the file
 *  \return ZFS_*
 */
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

	return zfs_setattr(&fa, fh, &args.attr, true);
}

/*! \brief set the file size to 0
 *
 *  \param fh handle to file
 *  \return ZFS_*
 */
static int zfs_truncate_file(zfs_fh * fh)
{
	// truncate file
	return zfs_set_end_of_file(fh, 0);
}

/*! \brief inner implementation of \ref zfs_dokan_create_file */
static int  inner_dokan_create_file (
	char * unix_path,      	// FileName
	DWORD desired_access,        	// DesiredAccess
	DWORD shared_mode,        	// ShareMode
	DWORD creation_disposition,    // CreationDisposition
	DWORD flags_and_attributes,    // FlagsAndAttributes
	PDOKAN_FILE_INFO info)
{
	int rv = zfs_file_exists(unix_path);
	if (rv == ENAMETOOLONG) return zfs_err_to_dokan_err(rv);
	bool_t file_exists = (rv == ZFS_OK);

	if (file_exists)
	{
		switch(creation_disposition)
		{
		case CREATE_NEW:
			return -ERROR_FILE_EXISTS;
		default:
			break;
		}
	}
	else
	{
		switch (creation_disposition) {
		case OPEN_EXISTING:
			return -ERROR_FILE_NOT_FOUND;
		case TRUNCATE_EXISTING:
			return -ERROR_FILE_NOT_FOUND;
		default:
			break;
		}
	}

	if (creation_disposition == TRUNCATE_EXISTING && !file_exists)
	{
		return -ERROR_FILE_NOT_FOUND;
	}

	char * path = "";
	char * name = "";

	if (creation_disposition == OPEN_EXISTING || creation_disposition == TRUNCATE_EXISTING
		|| (creation_disposition == CREATE_ALWAYS && file_exists)
		|| (creation_disposition == OPEN_ALWAYS && file_exists))
	{
		path = unix_path;
	}
	else
	{
		name = basename(unix_path);
		path = dirname(unix_path);
	}

	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	if (creation_disposition == OPEN_EXISTING || creation_disposition == TRUNCATE_EXISTING
		|| (creation_disposition == CREATE_ALWAYS && file_exists)
		|| (creation_disposition == OPEN_ALWAYS && file_exists))
	{
		if (creation_disposition == TRUNCATE_EXISTING || creation_disposition == CREATE_ALWAYS)
		{
			// truncate file
			//TODO: return value ??
			zfs_truncate_file(&lres.file);
		}

		zfs_cap local_cap;
		uint32_t flags = 0;
		convert_dokan_access_to_flags(&flags, desired_access);
		rv = zfs_open(&local_cap, &lres.file, flags); 
		if (rv != ZFS_OK)
		{
			return zfs_err_to_dokan_err(rv);
		}

		info->IsDirectory = (lres.attr.type == FT_DIR);
		// allocate memory for new capability
		zfs_cap *cap = xmemdup(&local_cap, sizeof(*cap));
		cap_to_dokan_file_info(info, cap);

		if (creation_disposition == OPEN_ALWAYS || creation_disposition == CREATE_ALWAYS)
		{
			// form dokan.h   you should return ERROR_ALREADY_EXISTS(183) (not negative value)
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

/*! \brief implements function CreateFile from win32api
 *
 *  \param file_name
 *  \param desired_access
 *  \param shared_mode
 *  \param creation_disposition
 *  \param flags_and_attributes
 *  \param info dokan's file handle
 *  \return -ERROR_
 *
 *  CreateFile (notes form win32api reference)
 *  If file is a directory, CreateFile (not OpenDirectory) may be called.
 *  In this case, CreateFile should return 0 when that directory can be opened.
 *  You should set TRUE on DokanFileInfo->IsDirectory when file is a directory.
 *  When CreationDisposition is CREATE_ALWAYS or OPEN_ALWAYS and a file already exists,
 *  you should return ERROR_ALREADY_EXISTS(183) (not negative value)
 */
static int  DOKAN_CALLBACK zfs_dokan_create_file (
	LPCWSTR file_name,      	// FileName
	DWORD desired_access,        	// DesiredAccess
	DWORD shared_mode,        	// ShareMode
	DWORD creation_disposition,    // CreationDisposition
	DWORD flags_and_attributes,    // FlagsAndAttributes
	PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_create_file(
				unix_path,
				desired_access,
				shared_mode,
				creation_disposition,
				flags_and_attributes,
				info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC

	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_open_directory */
static int inner_dokan_open_directory (
	char * unix_path,	// FileName
	PDOKAN_FILE_INFO info)
{

	int32_t rv;

	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, unix_path);
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

/*! \brief implements function OpenDirectory from win32api */
static int DOKAN_CALLBACK zfs_dokan_open_directory (
	LPCWSTR file_name,			// FileName
	PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_open_directory(
			unix_path,
			info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC

	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_create_directory */
static int inner_dokan_create_directory (
	char * unix_path,			// FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{

	int rv;
	char * name = basename(unix_path);
	char * path = dirname(unix_path);

	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	mkdir_args args;

	args.where.dir = lres.file;
	args.attr.mode = get_default_directory_mode();
	args.attr.uid = get_default_node_uid();
	args.attr.gid = get_default_node_gid();

	dir_op_res res;
	xmkstring(&args.where.name, name);
	rv = zfs_mkdir(&res, &args.where.dir, &args.where.name, &args.attr);
	xfreestring(&args.where.name);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	return -ERROR_SUCCESS;
}


/*! \brief implements function CreateDirectory from win32api */
static int DOKAN_CALLBACK zfs_dokan_create_directory (
	ATTRIBUTE_UNUSED LPCWSTR file_name,			// FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_create_directory(
			unix_path,
			info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC

	return rv;
}

/*! \brief implements Cleanup from dokan interface 
 *
 * When FileInfo->DeleteOnClose is true, you must delete the file in Cleanup.
*/
static int DOKAN_CALLBACK zfs_dokan_cleanup (
	ATTRIBUTE_UNUSED LPCWSTR file_name,      // FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_SUCCESS;
}

/*! \brief inner implementation of \ref zfs_dokan_close_file */
static int inner_dokan_close_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,      // FileName
	PDOKAN_FILE_INFO info)
{
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

/*! \brief implements function CloseFile from win32api */
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

/*! \brief inner implementation of \ref zfs_dokan_read_file */
static int inner_dokan_read_file (
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

/*! \brief implements function ReadFile from win32api */
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

/*! \brief inner implementation of \ref zfs_dokan_write_file */
static int inner_dokan_write_file (
	ATTRIBUTE_UNUSED LPCWSTR file_name,  // FileName
	LPCVOID buffer,  // Buffer
	DWORD number_of_bytes_to_write,    // NumberOfBytesToWrite
	LPDWORD number_of_bytes_written,  // NumberOfBytesWritten
	LONGLONG offset, // Offset
	PDOKAN_FILE_INFO info)
{

	*number_of_bytes_written = 0;
	write_args args;
	args.cap = * dokan_file_info_to_cap(info);
	while (number_of_bytes_to_write > 0)
	{
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

/*! \brief implements function WriteFile from win32api */
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

/*! \brief implements function FlushFileBuffers from win32api */
static int DOKAN_CALLBACK zfs_dokan_flush_file_buffers (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_SUCCESS;
}

/*! \brief inner implementation of \ref zfs_dokan_get_file_information */
static int inner_dokan_get_file_information (
	char * unix_path,          // FileName
	LPBY_HANDLE_FILE_INFORMATION buffer, // Buffer
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	TRACE("");

	dir_op_res lres;
	int rv = dokan_zfs_extended_lookup(&lres, unix_path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	fattr fa;
	rv = zfs_getattr(&fa, &lres.file);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	fattr_to_file_information(buffer, &fa);

	// unique file index compound from inode and volume id
	buffer->nFileIndexLow = lres.file.ino;
	buffer->nFileIndexHigh = lres.file.vid;

	return -ERROR_SUCCESS;
}

/*! \brief implements function GetFileInformation from win32api */
static int DOKAN_CALLBACK zfs_dokan_get_file_information (
	LPCWSTR file_name,          // FileName
	LPBY_HANDLE_FILE_INFORMATION buffer, // Buffer
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_get_file_information(
			unix_path,
			buffer,
			info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC

	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_find_files */
static int inner_dokan_find_files (
	ATTRIBUTE_UNUSED LPCWSTR path_name,	// PathName
	PFillFindData fill_data,		// call this function with PWIN32_FIND_DATAW
	PDOKAN_FILE_INFO info)  		//  (see PFillFindData definition)
{
	TRACE("");

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

			// don't return . or .. for root direcotry 
			if (ZFS_FH_EQ(cap->fh, root_fh))
			{
				if (strcmp(entry->name.str, ".") == 0)
					continue;

				if (strcmp(entry->name.str, "..") == 0)
					continue;
			}

			dir_op_res lookup_res;
			rv = zfs_extended_lookup(&lookup_res, &cap->fh, entry->name.str);
			if (rv != ZFS_OK)
			{
				continue;
			}

			 WIN32_FIND_DATAW find_data;
			 fattr_to_find_dataw(&find_data, &lookup_res.attr);
			 unix_to_windows_filename(entry->name.str, find_data.cFileName, MAX_PATH);
			 unix_to_alternative_filename(entry, find_data.cAlternateFileName);

			 int is_full = fill_data(&find_data, info);
			 if (is_full == 1)
			 {
				RETURN_INT(-ERROR_SUCCESS);
			 }
		}
	} while (list.eof == false);

	RETURN_INT(-ERROR_SUCCESS);
}

/*! \brief implements function FindFiles from Dokan interface */
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

/*! \brief converts windows file attributes and set them to a file on zlomekFS volume */
static int zfs_set_file_attributes(zfs_fh * fh, DWORD file_attributes, bool isDirectory)
{
	fattr fa;
	int rv = zfs_getattr(&fa, fh);
	if (rv != ZFS_OK)
	{
		return rv;
	}

	setattr_args args;
	if (FILE_ATTRIBUTE_READONLY & file_attributes)
	{
		if (isDirectory) return EINVAL;
		// get_default_file_ro_mode()
		args.attr.mode = fa.mode & (~(S_IWUSR | S_IWGRP | S_IWOTH));
	}
	else
	{
		args.attr.mode = get_default_file_mode();
	}

	args.attr.size = -1;
	args.attr.uid = -1;
	args.attr.gid = -1;
	args.attr.atime = -1;
	args.attr.mtime = -1;

	return zfs_setattr(&fa, fh, &args.attr, true);

}

/*! \brief inner implementation of \ref zfs_dokan_set_file_attributes */
static int inner_dokan_set_file_attributes (
	ATTRIBUTE_UNUSED LPCWSTR file_name, // FileName
	ATTRIBUTE_UNUSED DWORD file_attributes,   // FileAttributes
	PDOKAN_FILE_INFO info)
{

	/* read only can be set only on files */

	zfs_cap * cap = dokan_file_info_to_cap(info);
	if (cap == NULL) return -ERROR_BAD_ARGUMENTS;

	int32_t rv = zfs_set_file_attributes(&cap->fh, file_attributes, info->IsDirectory);
	return zfs_err_to_dokan_err(rv);
}

/*! \brief implements function SetFileAttributes from win32api */
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

/*! \brief inner implementation of \ref zfs_dokan_set_file_time */
static int inner_dokan_set_file_time (
        char * unix_path,                // FileName
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

	dir_op_res lres;
	int rv = dokan_zfs_extended_lookup(&lres, unix_path);
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

/*! \brief implements function SetFileTime from win32api */
static int DOKAN_CALLBACK zfs_dokan_set_file_time (
        LPCWSTR file_name,                // FileName
        CONST FILETIME* creation_time, // CreationTime
        CONST FILETIME* last_access_time, // LastAccessTime
        CONST FILETIME* last_write_time, // LastWriteTime
        PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_set_file_time(
				unix_path,
				creation_time,
				last_access_time,
				last_write_time,
				info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_delete_file */
static int inner_dokan_delete_file (
	char * unix_path,
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	int32_t rv;
	int f_type = zfs_get_file_type(unix_path);

	char * name = basename(unix_path);
	char * path = dirname(unix_path);

	dir_op_res lres;
	rv = dokan_zfs_extended_lookup(&lres, path);
	if (rv != ZFS_OK)
	{
		return zfs_err_to_dokan_err(rv);
	}

	dir_op_args args;
	args.dir = lres.file;
	xmkstring(&args.name, name);
	/*workaround dokan bug*/
	if (f_type == FT_DIR)
	{
		rv = zfs_rmdir(&args.dir, &args.name);
	}
	else
	{
		rv = zfs_unlink(&args.dir, &args.name);
	}
	xfreestring(&args.name);
	return zfs_err_to_dokan_err(rv);
}

/*! \brief implements function DeleteFile from win32api 
 *
 * notes form win32api reference:
 * You should not delete file on DeleteFile or DeleteDirectory.
 * When DeleteFile or DeleteDirectory, you must check whether
 * you can delete the file or not, and return 0 (when you can delete it)
 * or appropriate error codes such as -ERROR_DIR_NOT_EMPTY,
 * -ERROR_SHARING_VIOLATION.
 * When you return 0 (ERROR_SUCCESS), you get Cleanup with
 * FileInfo->DeleteOnClose set TRUE and you have to delete the
 * file in Close.
 */
static int DOKAN_CALLBACK zfs_dokan_delete_file (
	LPCWSTR file_name,
	PDOKAN_FILE_INFO info)
{
	int rv;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN
	
	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_delete_file(
			unix_path,
			info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC

	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_delete_directory */
static int inner_dokan_delete_directory ( 
	char * unix_path,
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	
	char * name = basename(unix_path);
	char * path = dirname(unix_path);

	dir_op_res lres;
	int rv = dokan_zfs_extended_lookup(&lres, path);
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

/*! \brief implements function DeleteDirectory from win32api */
static int DOKAN_CALLBACK zfs_dokan_delete_directory ( 
	LPCWSTR file_name, // FileName
	PDOKAN_FILE_INFO info)
{
	int rv = -ERROR_SUCCESS;

	DOKAN_SET_THREAD_SPECIFIC

	DOKAN_CONVERT_PATH_TO_UNIX_BEGIN

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_delete_directory(
			unix_path,
			info);
	}

	DOKAN_CONVERT_PATH_TO_UNIX_END

	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_move_file */
static int inner_dokan_move_file (
	char * existing_unix_path, // ExistingFileName
	char * new_unix_path, // NewFileName
	BOOL replace_existing,	// ReplaceExisiting
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	int rv;

	// chack if new file already exists
	if (replace_existing == FALSE)
	{
		rv = zfs_file_exists(new_unix_path);
		switch (rv)
		{
			case ZFS_OK:
				break;
			case ENAMETOOLONG:
				return zfs_err_to_dokan_err(rv);
			default:
				return -ERROR_ALREADY_EXISTS;
		}
	}

	char * existing_name = basename(existing_unix_path);
	char * existing_path = dirname(existing_unix_path);

	dir_op_res existing_lres;
 	rv = dokan_zfs_extended_lookup(&existing_lres, existing_path);
	if (rv != ZFS_OK) return zfs_err_to_dokan_err(rv);

	char * new_name = basename(new_unix_path);
	char * new_path = dirname(new_unix_path);

	dir_op_res new_lres;
 	rv = dokan_zfs_extended_lookup(&new_lres, new_path);
	if (rv != ZFS_OK) return zfs_err_to_dokan_err(rv);

	string s_existing_name;
	xmkstring(&s_existing_name, existing_name);
	string s_new_name;
	xmkstring(&s_new_name, new_name);

	rv = zfs_rename(&existing_lres.file, &s_existing_name, &new_lres.file, &s_new_name);

	xfreestring(&s_existing_name);
	xfreestring(&s_new_name);

	return zfs_err_to_dokan_err(rv);
}

/*! \brief implements function MoveFile from win32api */
static int DOKAN_CALLBACK zfs_dokan_move_file (
	LPCWSTR existing_file_name, // ExistingFileName
	LPCWSTR new_file_name, // NewFileName
	BOOL replace_existing,	// ReplaceExisiting
	PDOKAN_FILE_INFO info)
{
	DOKAN_SET_THREAD_SPECIFIC

	int rv = -ERROR_SUCCESS;
	char * existing_unix_path = NULL;
	char * new_unix_path = NULL;
	//char new_unix_path[ZFS_MAXPATHLEN];

	/*checks filesystem limits*/
	size_t existing_path_len = wcslen(existing_file_name);
	if (!DOKAN_CHECK_PATH_LIMIT(existing_path_len)) return zfs_err_to_dokan_err(ENAMETOOLONG);

	size_t new_path_len = wcslen(new_file_name);
	if (!DOKAN_CHECK_PATH_LIMIT(new_path_len)) return zfs_err_to_dokan_err(ENAMETOOLONG);

	existing_unix_path = xmalloc(sizeof(char) * (ZFS_MAXPATHLEN + 1));
	rv = windows_to_unix_path(existing_file_name, existing_unix_path, ZFS_MAXPATHLEN + 1);
	if (rv != ZFS_OK) rv = zfs_err_to_dokan_err(rv);

	if (rv == -ERROR_SUCCESS)
	{
		new_unix_path = xmalloc((sizeof(char) * (ZFS_MAXNAMELEN + 1)));
		rv = windows_to_unix_path(new_file_name, new_unix_path, ZFS_MAXPATHLEN + 1);
		if (rv != ZFS_OK) rv = zfs_err_to_dokan_err(rv);
	}

	if (rv == -ERROR_SUCCESS)
	{
		rv = inner_dokan_move_file(
			existing_unix_path,
			new_unix_path,
			replace_existing,
			info);
	}


	if (existing_unix_path != NULL) xfree(existing_unix_path);
	if (new_unix_path != NULL) xfree(new_unix_path);

	DOKAN_CLEAN_THREAD_SPECIFIC
	return rv;
}

/*! \brief inner implementation of \ref zfs_dokan_set_end_of_file */
static int inner_dokan_set_end_of_file (
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
	//TODO: fix dokaniface, it does not handle this return value properly, it returns instead of this vale FILE_NOT_FOUND
	if (rv == EINVAL) return -ERROR_INVALID_PARAMETER;

	return zfs_err_to_dokan_err(rv);
}

/*! \brief implements function SetEndOfFile from win32api */
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

/*! \brief inner implementation of \ref zfs_dokan_set_allocation_size */
static int inner_dokan_set_allocation_size (
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
		if (rv == EINVAL) return -ERROR_INVALID_PARAMETER;
		return zfs_err_to_dokan_err(rv);
	}

	return -ERROR_SUCCESS;
}


/*! \brief implements function SetAllocationSize from Dokan interface */
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

/*! \brief inner implementation of \ref zfs_dokan_get_volume_information */
static int inner_dokan_get_volume_information (
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

/*! \brief implements function GetVolumeInformation from win32api */
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

/*! \brief implements function Unmount from Dokan interface */
static int DOKAN_CALLBACK zfs_dokan_unmount (
	ATTRIBUTE_UNUSED PDOKAN_FILE_INFO info)
{
	return -ERROR_SUCCESS;
}

/*! \brief dokan operations structure with zlomekFS implementation */
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
	.LockFile = NULL,
	.UnlockFile = NULL,
	.GetFileSecurity = NULL,
	.SetFileSecurity = NULL,
	.GetDiskFreeSpace = NULL,
	.GetVolumeInformation = zfs_dokan_get_volume_information,
	.Unmount = zfs_dokan_unmount
};

/*! \brief dokan-iface main thread implementation
 *
 *  Setup thread enviroment, launch DokanMain form Dokan library,
 *  log return value and exit.
 *  \returns NULL
 */
static void * dokan_main(ATTRIBUTE_UNUSED void * data)
{
	thread_disable_signals();

	lock_info li[MAX_LOCKED_FILE_HANDLES];
	set_lock_info(li);

	// pass zlomek options to DOKAN OPTIONS
	mbstowcs(dokan_mount_point, zfs_config.mountpoint, ZFS_MAXNAMELEN);
	size_t thread_count = zfs_config.threads.kernel_thread_limit.max_total;

	if (thread_count > 0)
	{
		zfs_dokan_options.ThreadCount = thread_count; 
	}

#ifdef DOKAN_SINGLE_THREAD
	// for debuging purpose
	zfs_dokan_options.ThreadCount = 1; 
#endif

	mounted = true;
	int status = DokanMain(&zfs_dokan_options, &zfs_dokan_operations);
        switch (status) {
        case DOKAN_SUCCESS:
                message(LOG_NOTICE, FACILITY_ZFSD, "%s:Success\n", __func__);
                break;
        case DOKAN_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Error\n", __func__);
                break;
        case DOKAN_DRIVE_LETTER_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Bad Drive letter\n", __func__);
                break;
        case DOKAN_DRIVER_INSTALL_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Can't install driver\n", __func__);
                break;
        case DOKAN_START_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Driver something wrong\n", __func__);
                break;
        case DOKAN_MOUNT_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Can't assign a drive letter\n", __func__);
                break;
        case DOKAN_MOUNT_POINT_ERROR:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Mount point error\n", __func__);
                break;
        default:
                message(LOG_ERROR, FACILITY_ZFSD, "%s:Unknown error: %d\n", __func__, status);
                break;
        }

	mounted = false;

	/* notify zfsd daemon about fuse thread termination */
	pid_t pid = getpid();
	kill(pid, SIGTERM);

	return NULL;
}

/*! \brief export filesystem to OS
 *
 *  Part of fs-iface implementation, export filesystem to OS.
 *  \return true on success
 *  \return false in case of error
 */
bool fs_start(void)
{
	int rv = pthread_create(&dokan_thread, NULL, dokan_main, NULL);
	return (rv != -1);
}

/*! \brief disconnect filesystem from exported volumes
 *
 *  Part of fs-iface implementation, disconnect filesystem from exported volumes.
 */
void fs_unmount(void)
{
	if (mounted)
	{
		DokanUnmount(zfs_dokan_options.MountPoint[0]);
		mounted = false;
	}
}

/*! \brief cleanup dokan-iface internal structures
 *
 *  Part of fs-iface implementation, cleanup internal data structures in dokan-iface.
 */
void fs_cleanup(void)
{
	// nothing to do there
}

/*! \brief remove fh from kernel dentry cache
 *
 *  Part of fs-iface implementation, invalidate fh in kernel dentry cache, in case of dokan-iface do nothing.
 */
int32_t fs_invalidate_fh(ATTRIBUTE_UNUSED zfs_fh * fh)
{
	if (!mounted)
		RETURN_INT(ZFS_COULD_NOT_CONNECT);

	RETURN_INT(ZFS_OK);
}

/*! \brief invalidate kernel dentry cache 
 *
 *  Part of fs-iface implementation, invalidate kernel dentry cache, in case of dokan-iface do nothing. 
 */
int32_t fs_invalidate_dentry(ATTRIBUTE_UNUSED internal_dentry dentry, ATTRIBUTE_UNUSED bool volume_root_p)
{
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
	release_dentry(dentry);

	if (!mounted)
		RETURN_INT(ZFS_COULD_NOT_CONNECT);

	RETURN_INT(ZFS_OK);
}

