/*! 
 *  \file zfsio.h
 *  \brief Emulates stdio file api on zlomekFS file api.
 *  \author Ales Snuparek
 *
 *  Implements readonly support for stdio file api, which use FILE * on
 *  zlomekfs api which use zfs_fh * as file destciptor
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

#include <stdio.h>
#include "fh.h"

#ifndef ZFSIO_H
#define ZFS_IO_H

/*! \brief just a typecast */
typedef struct zfs_file_def zfs_file;

/*! \brief fopen wrapper */
zfs_file * zfs_fopen(zfs_fh * fh);

/*! \brief fclose wrapper */
int zfs_fclose(zfs_file * file);

/*! \brief converts zfs_file * to FILE */
FILE * zfs_fdget(zfs_file * file);

#endif

