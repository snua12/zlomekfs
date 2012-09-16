#ifndef DBUS_ZFSD_SERVICE_H
#define DBUS_ZFSD_SERVICE_H

/* ! \file \brief ZFS dbus provider.  */

/* Copyright (C) 2007 Jiri Zouhar

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

#include "dbus-service-descriptors.h"
#include "dbus-provider.h"
#include "system.h"

/// timeout for receiving message in miliseconds
#define DBUS_CONNECTION_TIMEOUT		1000

#define	ZFSD_DBUS_SIGNAL_MATCH_RULE	"type='signal',interface='" ZFSD_DBUS_INTERFACE "'"

/** Register zfsd names to dbus connection
 *
 * @see dbus_name_add_t
*/
int dbus_add_zfsd_name(DBusConnection * connection, DBusError * err_struct);

/** Release zfsd names from dbus connection
 *
 * @see dbus_name_release_t
*/
int dbus_release_zfsd_name(DBusConnection * connection,
						   DBusError * err_struct);

/** Try to handle zfsd dbus message
 *
 * @see dbus_message_handler_t
 */
message_handle_state_e dbus_handle_zfsd_message(DBusConnection * conn,
												DBusError * err_struct,
												DBusMessage * msg);


#endif /* DBUS_ZFSD_SERVICE_H */
