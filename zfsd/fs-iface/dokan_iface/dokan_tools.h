/*! \file dokan_tools.h
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


#ifndef DOKAN_TOOLS
#define DOKAN_TOOLS

#include <windows.h>
#include <winbase.h>
#include <wchar.h>
#include <dokan.h>
#include <fileinfo.h>
#include "zfs-prot.h"

#ifdef __cplusplus
extern "C" {
#endif 

/*! \brief serial number of exported volume */
#define ZFS_VOLUME_SERIAL_NUMBER 0xdeadbeef

int zfs_err_to_dokan_err(int32_t err);

zfs_cap * dokan_file_info_to_cap(PDOKAN_FILE_INFO info);

void cap_to_dokan_file_info(PDOKAN_FILE_INFO info, zfs_cap * cap);

void create_args_fill_dokan_access(create_args * args, DWORD desired_access);

void create_args_fill_dokan_shared_mode(create_args * args, DWORD shared_mode);

void create_args_fill_dokan_creation_disposition(create_args * args, DWORD creation_disposition);

void create_args_fill_dokan_flags_and_attributes(create_args * args, DWORD flags_and_attributes);

void convert_dokan_access_to_flags(uint32_t * flags,  DWORD desired_access);

void fattr_to_file_information(LPBY_HANDLE_FILE_INFORMATION buffer, fattr * fa);

void fattr_to_find_dataw(PWIN32_FIND_DATAW data, fattr * fa);

void filetime_to_zfstime(zfs_time * ztime, CONST FILETIME* ftime);

void unix_to_windows_filename(const char * unix_filename, LPWSTR windows_filename, int windows_filename_len);

void unix_to_alternative_filename(dir_entry * entry, LPWSTR windows_filename);

int windows_to_unix_path(LPCWSTR win_path, char * unix_path, size_t unix_path_len);

size_t  wcslen_in_utf8(const wchar_t * wstr);

#ifdef __cplusplus
}
#endif 

#endif
