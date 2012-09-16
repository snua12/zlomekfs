/* ! \file \brief Functions for threads communicating with kernel.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef FUSE_IFACE_H
#define FUSE_IFACE_H

#define FUSE_USE_VERSION 26
#include <fuse_lowlevel.h>

#include "system.h"
#include "pthread-wrapper.h"
#include "thread.h"

/* ! Arguments from main (), after parsing the zfsd-specific options */
extern struct fuse_args main_args;

/* ! Pool of kernel threads (threads communicating with kernel).  */
extern thread_pool kernel_pool;

#endif
