/**
 *  \file control.h
 * 
 *  \author Ales Snuparek
 *  \brief Support for zlomekFS control interface.
 *
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

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

#ifndef CONTROL_H
#define CONTROL_H

#include "zfs-prot.h"

#ifdef __cplusplus
extern "C"
{
#endif


const char * connection_speed_to_str(connection_speed speed);

bool zfs_control_get_connection_forced(void);
void zfs_control_set_connection_forced(bool forced);

connection_speed zfs_control_get_connection_speed(void);
void zfs_control_set_connection_speed(connection_speed speed);

bool initialize_control_c(void);

void cleanup_control_c(void);

#ifdef __cplusplus
}
#endif

#endif
