/* Volume functions.
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

#include "system.h"
#include <inttypes.h>
#include <stdlib.h>
#include "memory.h"
#include "volume.h"

/* Create volume structure and fill it with information.  */
volume
volume_create (unsigned id)
{
  volume vol;

  vol = (volume) xmalloc (sizeof (volume));
  vol->id = id;
  vol->name = NULL;
  vol->master = NULL;
  vol->mountpoint = NULL;
  vol->flags = 0;
  vol->localpath = NULL;
  vol->size_limit = UINT64_MAX;

  return vol;
}
