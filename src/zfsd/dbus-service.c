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

#include <unistd.h>

#include "dbus-service.h"
#include "zfsd.h"
#include "log.h"


void reply_to_ping(DBusMessage* msg, DBusConnection* conn)
{
  DBusMessage* reply;
  DBusMessageIter args;

  // read the arguments
  if (dbus_message_iter_init(msg, &args))
    message (LOG_WARNING, FACILITY_DBUS, "Message ping has!\n"); 

  // create a reply from the message
  reply = dbus_message_new_method_return(msg);

  // add the arguments to the reply
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, ZFSD_STATUS_INFO_DBUS_TYPE, &zfsd_state)) { 
    message (LOG_WARNING, FACILITY_DBUS, "Out Of Memory!\n"); 
    return;
  }

  // send the reply && flush the connection
  if (!dbus_connection_send(conn, reply, NULL)) {
    message (LOG_WARNING, FACILITY_DBUS, "Out Of Memory!\n"); 
    return;
  }
  dbus_connection_flush(conn);

  // free the reply
  dbus_message_unref(reply);
}


/**
 * Server that exposes a method call and waits for it to be called
 */
void * dbus_service_loop(void * should_exit) 
{
  DBusMessage* msg;
  DBusConnection* conn;
  DBusError err;
  int ret;

  message (LOG_TRACE, FACILITY_DBUS, "Listening for method calls\n");

  // initialise the error
  dbus_error_init (&err);
  
  // connect to the bus and check for errors
  conn = dbus_bus_get (DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set (&err)) { 
    message (LOG_ERROR, FACILITY_DBUS, "Connection Error (%s)\n", err.message); 
    dbus_error_free (&err); 
  }
  if (NULL == conn) {
    message (LOG_ERROR, FACILITY_DBUS, "Connection Null\n"); 
    return NULL;
  }
  
  // request our name on the bus and check for errors
  ret = dbus_bus_request_name (conn, ZFSD_DBUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
  if (dbus_error_is_set (&err)) { 
    message (LOG_ERROR, FACILITY_DBUS, "Name Error (%s)\n", err.message); 
    dbus_error_free (&err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
    message (LOG_ERROR, FACILITY_DBUS, "Not Primary Owner (%d)\n", ret);
    return NULL;
  }


  // add a rule for which messages we want to see
  dbus_bus_add_match (conn, "type='signal',interface='" ZFSD_DBUS_INTERFACE "'", &err); // see signals from the given interface
  dbus_connection_flush (conn);
  if (dbus_error_is_set (&err)) { 
    message (LOG_ERROR, FACILITY_DBUS, "Match Error (%s)\n", err.message);
    return NULL;
  }
  message (LOG_TRACE, FACILITY_DBUS, "Match rule sent\n");

  // loop, testing for new messages
  while ((*((bool_t*)should_exit)) != TRUE) {
    // non blocking read of the next available message
    dbus_connection_read_write (conn, DBUS_CONNECTION_TIMEOUT);
    msg = dbus_connection_pop_message (conn);

    // loop again if we haven't got a message
    if (NULL == msg) {  
       sleep (DBUS_CONNECTION_TIMEOUT / 1000);
       continue; 
    }
    
    // check this is a method call for the right interface & method
    if (dbus_message_is_method_call (msg, ZFSD_DBUS_INTERFACE, 
                                    ZFSD_STATUS_INFO_MESSAGE_NAME))
      reply_to_ping (msg, conn);

    // free the message
    dbus_message_unref (msg);
  }

  dbus_bus_release_name (conn, ZFSD_DBUS_NAME, NULL);
  dbus_connection_unref (conn);

}


