/*! \file
    \brief ZFS dbus provider.  */

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

#ifndef DBUS_SERVICE_H
#define DBUS_SERVICE_H

#include "dbus-service-descriptors.h"
#include "system.h"

/// timeout for receiving message in miliseconds
#define DBUS_CONNECTION_TIMEOUT		1000

#define	ZFSD_DBUS_SIGNAL_MATCH_RULE	"type='signal',interface='" ZFSD_DBUS_INTERFACE "'"

/** open dbus connection, listen for messages and reply
    finalize connection upon exit
 *
 * @param data int * pointer if set to TRUE, the loop will terminate
 * @return NULL
 */
void * dbus_zfsd_service_loop (void * data);

/** Register zfsd names to dbus connection
 *
 * @param connection initialized dbus connection
 * @param err_struct initialized dbus error struct 
 * @return TRUE if successfully added, FALSE otherwise
 *
*/
int dbus_add_zfsd_name (DBusConnection * connection, 
                                DBusError * err_struct);

/** Release zfsd names from dbus connection
 *
 * @param connection initialized dbus connection
 * @param err_struct initialized dbus error struct 
 * @return TRUE if successfully released, FALSE otherwise
 *
*/
int dbus_release_zfsd_name (DBusConnection * connection,
                                    DBusError * err_struct);

typedef enum {
  ZFSD_MESSAGE_HANDLED = 0,
  ZFSD_MESSAGE_UNKNOWN = 1,
  ZFSD_HANDLE_ERROR = 2
} message_handle_state_e;

/** Try to handle dbus message
 * @param conn valid dbus connection with syplog names registered
 * @param err_struct initialized dbus error struct
 * @param msg message received
 * @return ZFSD_MESSAGE_HANDLED if handled,
           ZFSD_HANDLE_ERROR if message is known bud error occured in processing,
           ZFSD_MESSAGE_UKNONWN if zfsd doesn't known message type
 */
message_handle_state_e dbus_handle_zfsd_message (DBusConnection * conn, 
                                                 DBusError * err_struct,
                                                 DBusMessage * msg);


#endif /* DBUS_SERVICE_H */
