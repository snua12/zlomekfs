/* Directory operations.
   Copyright (C) 2003 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef DIR_H
#define DIR_H

#include "system.h"
#include "fh.h"
#include "zfs_prot.h"
#include "volume.h"

extern char *build_local_path (volume vol, internal_fh fh);
extern char *build_local_path_name (volume vol, internal_fh fh,
				    const char *name);
extern int update_volume_root (volume vol, internal_fh *ifh);
extern int zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path);
extern int zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name);

#endif
