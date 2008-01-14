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

#include "dbus-zfsd-service.h"
#include "zfsd.h"
#include "log.h"


message_handle_state_e reply_to_ping(DBusMessage* msg, DBusConnection* conn)
{
  DBusMessage* reply;
  DBusMessageIter args;

  // create a reply from the message
  reply = dbus_message_new_method_return(msg);

  // add the arguments to the reply
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, ZFSD_STATUS_INFO_DBUS_TYPE, &zfsd_state)) { 
    message (LOG_WARNING, FACILITY_DBUS, "Out Of Memory!\n"); 
    return ZFSD_HANDLE_ERROR;
  }

  // send the reply && flush the connection
  if (!dbus_connection_send(conn, reply, NULL)) {
    message (LOG_WARNING, FACILITY_DBUS, "Out Of Memory!\n"); 
    return ZFSD_HANDLE_ERROR;
  }
  dbus_connection_flush(conn);

  // free the reply
  dbus_message_unref(reply);

  return ZFSD_MESSAGE_HANDLED;
}

/// Register zfsd names to dbus connection
int dbus_add_zfsd_name (DBusConnection * connection, 
                        DBusError * err_struct)
{
  int ret = 0;
  // request our name on the bus and check for errors
  ret = dbus_bus_request_name (connection, ZFSD_DBUS_NAME,
                               DBUS_NAME_FLAG_REPLACE_EXISTING, err_struct);
  if (dbus_error_is_set (err_struct)) 
  { 
    message (LOG_ERROR, FACILITY_DBUS | FACILITY_ZFSD, "Name Error (%s)\n",
             err_struct->message);
    dbus_error_free (err_struct);
  }

  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
    message (LOG_ERROR, FACILITY_DBUS | FACILITY_ZFSD,
             "Not Primary Owner (%d)\n", ret);
    return FALSE;
  }

  //NOTE: we dont' use any signals yet - this is redundant
  // add a rule for which messages we want to see
  dbus_bus_add_match (connection, ZFSD_DBUS_SIGNAL_MATCH_RULE, err_struct);
  dbus_connection_flush (connection);
  if (dbus_error_is_set (err_struct)) 
  { 
    message (LOG_ERROR, FACILITY_DBUS | FACILITY_ZFSD, "Match Error (%s)\n",
             err_struct->message);
    dbus_error_free (err_struct);
    return FALSE;
  }
  message (LOG_TRACE, FACILITY_DBUS | FACILITY_ZFSD, "Match rule sent\n");

  return TRUE;
}

/// Release zfsd names from dbus connection
int dbus_release_zfsd_name (DBusConnection * connection,
                            DBusError * err_struct)
{
  int ret_code = TRUE;

  //NOTE: we dont' use any signals yet - this is redundant
  dbus_bus_remove_match (connection, ZFSD_DBUS_SIGNAL_MATCH_RULE, err_struct);
  if (dbus_error_is_set(err_struct)) { 
    message (LOG_WARNING, FACILITY_LOG | FACILITY_DBUS, 
            "Can't unergister zfsd dbus signal match (%s)\n",
            err_struct->message);
    dbus_error_free (err_struct);
    ret_code = FALSE;
  }

  dbus_bus_release_name (connection, ZFSD_DBUS_NAME, err_struct);
  if (dbus_error_is_set(err_struct)) { 
    message (LOG_WARNING, FACILITY_LOG | FACILITY_DBUS,
            "Can't release zfsd dbus name (%s)\n",
            err_struct->message);
    dbus_error_free (err_struct);
    ret_code = FALSE;
  }

  return ret_code;
}

/// Try to handle dbus message
message_handle_state_e dbus_handle_zfsd_message (DBusConnection * conn, 
                                                 DBusError * err_struct ATTRIBUTE_UNUSED,
                                                 DBusMessage * msg)
{
    // check this is a method call for the right interface & method
    if (dbus_message_is_method_call (msg, ZFSD_DBUS_INTERFACE, 
                                    ZFSD_STATUS_INFO_MESSAGE_NAME))
      return reply_to_ping (msg, conn);

  return ZFSD_MESSAGE_UNKNOWN;
}


/**
 * Server that exposes a method call and waits for it to be called
 */
void * dbus_zfsd_service_loop(void * should_exit) 
{
  DBusMessage* msg = NULL;
  DBusConnection* conn = NULL;
  DBusError err;

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
    goto FINISHING;
  }
  

  if ( FALSE == dbus_add_zfsd_name (conn, &err))
  {
    goto FINISHING;
  }

  // loop, testing for new messages
  while ((*((int*)should_exit)) != TRUE) {
    // non blocking read of the next available message
    dbus_connection_read_write (conn, DBUS_CONNECTION_TIMEOUT);
    msg = dbus_connection_pop_message (conn);

    // loop again if we haven't got a message
    if (NULL == msg) {  
       sleep (DBUS_CONNECTION_TIMEOUT / 1000);
       continue; 
    }
    
    dbus_handle_zfsd_message (conn, &err, msg);

    // free the message
    dbus_message_unref (msg);
  }


FINISHING:
  if (conn)
  {
    dbus_release_zfsd_name (conn, &err);
    dbus_connection_unref (conn);
  }

  return NULL;

}


