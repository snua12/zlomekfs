/**
 *  \file control.c
 * 
 *  \author Ales Snuparek
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


#include "system.h"
#include <string.h>
#include "zfs-prot.h"
#include "dbus-init.h"
#include "log.h"
#include "control.h"

#ifdef ENABLE_CLI
#include "control_zfsd_cli.h"
#endif

typedef struct control_connection_def
{
	bool forced;
	connection_speed speed;
} control_connection;

#define CONTROL_CONNECTION_INITIALIZER { .forced = false, \
	.speed = CONNECTION_SPEED_NONE }

typedef struct control_def
{
	control_connection connection;
} control;

static control zfs_control = {.connection = CONTROL_CONNECTION_INITIALIZER};

/*! conversion table from enum connection_speed to string */
const char *  connection_speed_str[] = {
	"none",
	"slow",
	"fast",
	NULL
};

const char * connection_speed_to_str(connection_speed speed)
{
	if (speed < 0 || speed >= CONNECTION_SPEED_LAST_AND_UNUSED)
		return NULL;
	
	return connection_speed_str[speed];
}


connection_speed zfs_control_get_connection_speed(void)
{
	return zfs_control.connection.speed;
}

void zfs_control_set_connection_speed(connection_speed speed)
{
	zfs_control.connection.speed = speed;
}

bool zfs_control_get_connection_forced(void)
{
	return zfs_control.connection.forced;
}

void zfs_control_set_connection_forced(bool forced)
{
	zfs_control.connection.forced = forced;
}

bool initialize_control_c(void)
{
#ifdef ENABLE_DBUS
	start_dbus_control();
#endif

#ifdef ENABLE_CLI
	start_cli_control();
#endif
	return true;
}

void cleanup_control_c(void)
{
#ifdef ENABLE_DBUS
	stop_dbus_control();
#endif

#ifdef ENABLE_CLI
	stop_cli_control();
#endif
}
