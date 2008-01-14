/*! \file
    \brief ZFS dbus descriptors.  */

/* Copyright (C) 2007 Jiri Zouhar

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

#ifndef DBUS_SERVICE_DESCRIPTORS_H
#define DBUS_SERVICE_DESCRIPTORS_H

#include <dbus/dbus.h>

#define	ZFSD_DBUS_NAME			"zfsd.default"
#define	ZFSD_DBUS_INTERFACE		"zfsd.info"


#define SFSD_STATUS__INFO_MESSAGE_NAME	"status"
#define	ZFSD_STATUS_INFO_DBUS_TYPE	DBUS_TYPE_UINT32

typedef enum {
  ZFSD_STATE_STARTING = 0,
  ZFSD_STATE_RUNNING = 1,
  ZFSD_STATE_TERMINATING = 10,
  ZFSD_STATE_UNKNOWN = 11
} zfsd_state_e;


#endif /* DBUS_SERVICE_DESCRIPTORS_H */
