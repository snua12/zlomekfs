/*! \file dokan_tools.c
 *  \brief Dokan interface helper functions
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


#include <windows.h>
#include <winbase.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "dokan_tools.h"
#include "log.h"


/*! \brief constant for windows directory delimiter */
static const wchar_t windows_dir_delimiter[] = L"\\";

/*! \brief constant for unix directory delimiter */
static const char unix_dir_delimiter[] = "/";
static const size_t unix_dir_delimiter_len = (sizeof(unix_dir_delimiter) / sizeof(unix_dir_delimiter[0])) - 1;

/*! Similar to wcsdup but always returns valid pointer.  */
static wchar_t *xwcsdup(const wchar_t *s) 
{
	wchar_t *r = wcsdup(s);
	if (!r)
	{   
		message(LOG_ALERT, FACILITY_MEMORY, "Not enough memory.\n");
		zfsd_abort();
	}   
	return r;
}

static int windows_to_unix_path_no_const(LPWSTR win_path, char * unix_path, size_t unix_path_len)
{
	if (unix_path_len <= 1)
	{
		return ENAMETOOLONG;
	}
	
	// space for terminating \0
	unix_path_len -= 1;

	char * unix_path_ptr = unix_path;
	*unix_path_ptr = 0;
	(void) strcat(unix_path_ptr, unix_dir_delimiter);

	wchar_t * ctx;
	wchar_t * tok;

	for (tok = wcstok(win_path, windows_dir_delimiter, &ctx);
		tok != NULL;
		tok = wcstok(NULL, windows_dir_delimiter, &ctx))
	{
		size_t tok_len = wcslen(tok);
		if (tok_len == 0)
		{
			continue;
		}

		// how many char will be used in output unix_path string
		size_t unix_path_used = ((size_t) (unix_path_ptr - unix_path)) + tok_len + unix_dir_delimiter_len;

		if ((unix_path_len) < unix_path_used)
		{
			return ENAMETOOLONG;
		}
		
		// add unix directory delimiter
		(void) strcat(unix_path_ptr, unix_dir_delimiter);
		unix_path_ptr += unix_dir_delimiter_len;	

		int rv = WideCharToMultiByte(
				CP_UTF8,
				0,
				tok,
				tok_len,
				unix_path_ptr,
				(unix_path_len) - (unix_path_ptr - unix_path),	// free char in output unix_path_string
				NULL,
				NULL);

		// something goes wrong during conversion from UNICODE to UTF8
		if (rv == 0)
		{
			*unix_path_ptr = 0;
			return ENAMETOOLONG;
		}

		if (rv > ZFS_MAXNAMELEN) return ENAMETOOLONG;
		unix_path_ptr += rv;
		*unix_path_ptr = '\0';
	}

	return ZFS_OK;
}

/*! \brief converts windows path to unix path
 *
 */
int windows_to_unix_path(LPCWSTR win_path, char * unix_path, size_t unix_path_len)
{
	LPWSTR win_path_dup = xwcsdup(win_path);
	int rv = windows_to_unix_path_no_const(win_path_dup, unix_path, unix_path_len);
	xfree(win_path_dup);
	return rv;
}

/*! \brief converts errors from zlomekFS to win32api errors 
 *
 *  \param err zlomekFS error
 *  \return -1 * ERROR_* (win32api)
 *  error translation can be found at cygwin sources winsup/cygwin/errno.cc
*/
int zfs_err_to_dokan_err(int32_t err)
{
	switch (err)
	{
		case ZFS_OK:
			return -ERROR_SUCCESS;
		case ENOENT:
			return -ERROR_FILE_NOT_FOUND;
		case ENAMETOOLONG:
			return -ERROR_PATH_NOT_FOUND; /* The file name is too long. */
		case EROFS:
			return -ERROR_WRITE_PROTECT;
		case EEXIST:
			return -ERROR_ALREADY_EXISTS;
		case ENOTEMPTY:
			return -ERROR_DIR_NOT_EMPTY;
		case ENOTDIR:
			return -ERROR_DIRECTORY;
		case EINVAL:
			return -ERROR_INVALID_PARAMETER;
		default:
			message(LOG_WARNING, FACILITY_ZFSD, "%s:errno %d not translated\n", __func__, err);
			return -ERROR_INVALID_FUNCTION;
	}
}

/*! \brief converts PDOKAN_FILE_INFO to zfs_cap
 *
 *  \param info is PDOKAN_FILE_INFO dokan info
 *  \return zfs_cap (zlomekFS file handle)
 */
zfs_cap * dokan_file_info_to_cap(PDOKAN_FILE_INFO info)
{
	intptr_t context = info->Context;
	return (zfs_cap *) context;
}

/*! \brief converts zfs_cap to PDOKAN_FILE_INFO
 *
 *  \param info PDOKAN_FILE_INFO dokan info
 *  \param cap zfs_cap
 */
void cap_to_dokan_file_info(PDOKAN_FILE_INFO info, zfs_cap * cap)
{
	intptr_t context = (intptr_t) cap;
	info->Context = context;
}

/*! \brief converts win32api access flags to unix access rights
 *
 *  \param flags output unix flags
 *  \param desired_access win32api access flags
 */
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
		*flags = O_RDWR;
		return;
	}

	// this acces is used when is called function SetFileAttributes
	if (desired_access == 0x100100)
	{
		*flags = O_RDONLY;
		return;
	}

	message(LOG_ERROR, FACILITY_ZFSD, "%s:cannot convert desired access 0x%lx\n", __func__, desired_access);

	*flags = O_RDONLY;
}

/*! \brief fill create_args with win32api access rights
 *
 *  \param args pointer to create_args (place where are access rights filled)
 *  \param desired_access win32api access rights
 */
void create_args_fill_dokan_access(create_args * args, DWORD desired_access)
{

	convert_dokan_access_to_flags(&args->flags, desired_access);
}

/*! \brief fill create_args with win32api shared mode
 *
 *  this function has empty implementation
 *  \param args pointer to create_args
 *  \param shared_mode win32api shared mode
 */
void create_args_fill_dokan_shared_mode( ATTRIBUTE_UNUSED create_args * args, ATTRIBUTE_UNUSED DWORD shared_mode)
{
}

/*! \brief fill create_args with win32api access rights
 *
 *  \param args pointer to create_args
 *  \param creation_disposition win32api creation disposition
 */
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

/*! \brief fill create_args with win32api flags and attributes
 *
 *  this function has empty implementation
 *  \param args pointer to create_args
 *  \param flags_and_attributes win32api flags and attributes
 */
void create_args_fill_dokan_flags_and_attributes(ATTRIBUTE_UNUSED create_args * args, ATTRIBUTE_UNUSED DWORD flags_and_attributes)
{
}

/*! \brief converts unix time to windows time
 *
 *  based on microsoft kb 167296
 * \param ftime pointer to place where is windows time stored
 * \param ztime unix time representation
 */
static void zfstime_to_filetime(PFILETIME ftime, zfs_time ztime)
{
	if (ftime == NULL || ztime == ((zfs_time) - 1)) return;
	// based on microsoft kb 167296
	LONGLONG ll;
	ll = Int32x32To64(ztime, 10000000) + 116444736000000000;

	ftime->dwLowDateTime = (DWORD) ll;
	ftime->dwHighDateTime = ll >> 32;
}

/*! \brief converts windows time to unix time
 *
 *  based on microsoft kb 167296
 *  \param ztime pointer to place where is unix time stored
 *  \param ftime pointer to windows time representation
 */
void filetime_to_zfstime(zfs_time * ztime, CONST FILETIME* ftime)
{
	if (ftime == NULL || ztime == NULL) return;
	if (ftime->dwHighDateTime == 0 && ftime->dwLowDateTime == 0) return;

	LONGLONG ll = ftime->dwHighDateTime;
	ll = (ll << 32) + ftime->dwLowDateTime;
	*ztime = (zfs_time)((ll - 116444736000000000) / 10000000);

}

/*! \brief converts unix file type to win32api atributes
 *
 *  \param type unix file type
 *  \return win32api attributes
 */
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

static void mode_to_file_attrs(DWORD * attrs, uint32_t mode)
{
	if ((mode & S_IWUSR) || (mode & S_IWGRP) || (mode & S_IWOTH))
		return;
	/* A file that does not have other attributes set. This attribute is valid only when used alone. */
	*attrs = *attrs & ~FILE_ATTRIBUTE_NORMAL;
	*attrs |= FILE_ATTRIBUTE_READONLY;
}

/*! \brief converts zlomekFS fattr to win32api FILE INFORMATION
 *
 *  \param buffer pointer to place where is file information stored
 *  \param fa zlomekFS fattr
 */
void fattr_to_file_information(LPBY_HANDLE_FILE_INFORMATION buffer, fattr * fa)
{
	memset(buffer, 0, sizeof(*buffer));

	buffer->nFileSizeLow =  (DWORD) fa->size;
	buffer->nFileSizeHigh = (fa->size) >> 32;
	buffer->dwFileAttributes = ftype_to_file_attrs(fa->type);

	mode_to_file_attrs(&(buffer->dwFileAttributes), fa->mode);

	// use mtime instead of ctime (ctime cannot be altered by POSIX API)
	zfstime_to_filetime(&buffer->ftCreationTime, fa->mtime);
	zfstime_to_filetime(&buffer->ftLastAccessTime, fa->atime);
	zfstime_to_filetime(&buffer->ftLastWriteTime, fa->mtime);

	buffer->dwVolumeSerialNumber = ZFS_VOLUME_SERIAL_NUMBER;
	buffer->nNumberOfLinks = fa->nlink;
}

/*! \brief converts zlomekFS fattr to win32api PWIN32_FIND_DATAW
 *
 *  \param data pointer to place where is PWIN32_FIND_DATAW
 *  \param fa zlomekFS fattr
 */
void fattr_to_find_dataw(PWIN32_FIND_DATAW data, fattr * fa)
{
	memset(data, 0, sizeof(*data));

	data->nFileSizeLow =  (DWORD) fa->size;
	data->nFileSizeHigh = (fa->size) >> 32;

	data->dwFileAttributes |= ftype_to_file_attrs(fa->type);
	mode_to_file_attrs(&(data->dwFileAttributes), fa->mode);

	zfstime_to_filetime(&data->ftCreationTime, fa->ctime);
	zfstime_to_filetime(&data->ftLastAccessTime, fa->atime);
	zfstime_to_filetime(&data->ftLastWriteTime, fa->mtime);
}

/*! \brief converts UTF-8 encoded filename to UTF-16 encoded filename
 *
 *  \param unix_filename UTF-8 filename
 *  \param windows_filename UTF-16 filename representation
 *  \param windows_filename_len length of windows_filename
 */
void unix_to_windows_filename(const char * unix_filename, LPWSTR windows_filename, int windows_filename_len)
{
	int rv = MultiByteToWideChar(
		CP_UTF8,
		0,
		unix_filename,
		strlen(unix_filename),
		windows_filename,
		windows_filename_len
	);

	// add terminating 0
	if (rv > 0 && rv < windows_filename_len)
	{
		windows_filename[rv] = 0;
	}

	if (rv == 0)
	{
                message(LOG_ERROR, FACILITY_ZFSD, "%s:failed to conver unix_filename to windows_filename\n", __func__);
		wcsncpy(windows_filename, L"", windows_filename_len);
	}

}

/*! \brief converts UTF-8 encoded filename to UTF-16 encoded  8.3 filename
 *
 *  converts long file name to short filename. This is done unique filename is made from:
 *  filename_prefix~hex_encoded_inode_id.shorted_file_extension
 *  \param dir_entry directory entry
 *  \param windows_filename UTF-16 filename representation (must be at least 13 character long)
 */
void unix_to_alternative_filename(dir_entry * entry, LPWSTR windows_filename)
{
	size_t name_len = strlen(entry->name.str);
	if (name_len < 13)
	{
		unix_to_windows_filename(entry->name.str, windows_filename, 13);
		return;
	}

	// converts inode to hexadecimal string strlen(ino_str) < 9
	char ino_str[9];
	snprintf(ino_str, sizeof(ino_str), "%X", entry->ino);
	size_t ino_str_len = strlen(ino_str);

	// get tile extension if there is any
	char file_ext[ZFS_MAXNAMELEN + 1] = "";
	char * file_ext_p = strchr(entry->name.str, '.');
	if  (file_ext_p != NULL)
	{
		snprintf(file_ext, sizeof(file_ext), "%s",
			file_ext_p);
	}
	// limit file extension to 4 characters
	file_ext[4] = 0;
	size_t file_ext_len = strlen(file_ext);

	// get filename
	char file_name[ZFS_MAXNAMELEN + 1] = "";
	size_t file_name_len = file_ext_len + ino_str_len;
	if (file_name_len < 12)
	{
		snprintf(file_name, 13 - file_name_len,"%s",
			entry->name.str);
		file_name[12 - file_name_len - 1] = '~';	
	}

	// merge together (file_name hex_inode file_extension)
	char file_name_final[ZFS_MAXNAMELEN + 1] = "";
	snprintf(file_name_final, sizeof(file_name_final), "%s%s%s",
		file_name, ino_str, file_ext);

	// converts to windows encoding
	unix_to_windows_filename(file_name_final, windows_filename, 13);
}

#if 0
// for debugging purpose
/*! \brief converts create disposition constant to string representation
 *
 *  \param creation_disposition win32api create disposition
 *  \return string representation of win32api create disposition
 */
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

