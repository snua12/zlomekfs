/* Metadata management functions.
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

#ifndef METADATA_H
#define METADATA_H

#include "system.h"
#include "volume.h"
#include "fh.h"

/* Purpose of the interval tree.  */
typedef enum interval_tree_purpose_def
{
  /* Intervals updated from master node.  */
  INTERVAL_TREE_UPDATED,

  /* Intervals modified by local node.  */
  INTERVAL_TREE_MODIFIED
} interval_tree_purpose;

extern bool init_interval_tree (volume vol, internal_fh fh,
				interval_tree_purpose purpose);
extern bool flush_interval_tree (volume vol, internal_fh fh,
				 interval_tree_purpose purpose);

extern void initialize_metadata_c ();
extern void cleanup_metadata_c ();
#endif
