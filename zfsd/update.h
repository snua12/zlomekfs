/* Functions for updating files.
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

#ifndef UPDATE_H
#define UPDATE_H

#include "system.h"
#include <inttypes.h>
#include "varray.h"
#include "fh.h"
#include "cap.h"
#include "zfs_prot.h"

#define ZFS_UPDATED_BLOCK_SIZE ZFS_MAXDATA
#define ZFS_MODIFIED_BLOCK_SIZE 1024

/* Update directory DENTRY on volume VOL if needed.  */
#define UPDATE_DIR_IF_NEEDED(VOL, DENTRY)				\
  if ((VOL)->master != this_node)					\
    {									\
      if (update_p ((DENTRY), (VOL)))					\
	{								\
	  zfs_fh fh;							\
									\
	  CHECK_MUTEX_LOCKED (&(DENTRY)->fh->mutex);			\
	  CHECK_MUTEX_LOCKED (&(VOL)->mutex);				\
									\
	  fh = (DENTRY)->fh->local_fh;					\
	  r = update_file ((DENTRY), (VOL), false);			\
	  if (r != ZFS_OK)						\
	    return r;							\
									\
	  r = zfs_fh_lookup (&fh, &(VOL), &(DENTRY), NULL);		\
	  if (r != ZFS_OK)						\
	    return r;							\
	}								\
    }

extern void get_blocks_for_updating (internal_fh fh, uint64_t start,
				     uint64_t end, varray *blocks);
extern int32_t update_file_blocks (bool use_buffer, uint32_t *rcount,
				   void *buffer, uint64_t offset,
				   internal_cap cap, varray *blocks);
extern bool update_p (internal_dentry dentry, volume vol);
extern int32_t update_file (internal_dentry dentry, volume vol, bool schedule);

#endif
