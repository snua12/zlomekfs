/*! \file
    \brief Implementation of control client functions (high level logger-control interface)
    \see control.h
*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Syplog.

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
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#define _GNU_SOURCE
#include <unistd.h>
#undef _GNU_SOURCE

#include "control.h"
#include "errno.h"

/** resolve address to struct in_addr
 * @param addr IP address only so far
 * @param target valid pointer to struct in_addr where to store resolved address
 * @return ERR_BAD_PARAMS, NOERR
*/
syp_error resolve_host (const char * addr, struct in_addr * target)
{
 /* TODO: use
  int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res);
 */
 if (inet_aton(addr, target) == 0)
   return ERR_BAD_PARAMS;
 
 return NOERR;
 
}


syp_error send_uint32_by_function (uint32_t data, 
  syp_error (*function)(int, uint32_t, const struct sockaddr *, socklen_t), 
  const char * ip, uint16_t port)
{
  struct sockaddr_in addr;
  int sock = -1;
  syp_error ret_code = NOERR;
  
  if (ip ==  NULL)
    ip = DEFAULT_COMMUNICATION_ADDRESS;
  if (port == 0)
    port = DEFAULT_COMMUNICATION_PORT;
    
  // init socket
  sock = socket(AF_INET,SOCK_DGRAM,0);
  if (sock == -1)
  {
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  
  ret_code = resolve_host (ip, &(addr.sin_addr));
  if (ret_code != NOERR)
    goto FINISHING;
  
  ret_code = function (sock, data, (struct sockaddr *)&addr, sizeof (struct sockaddr_in));
  
FINISHING:
  if (sock >= 0)
    close (sock);
    
  return ret_code;
}

syp_error set_level_udp (log_level_t level, const char * addr, uint16_t port)
{
  return send_uint32_by_function (level, set_level_sendto, addr,port);
}

syp_error set_facility_udp (facility_t facility, const char * addr, uint16_t port)
{
  return send_uint32_by_function (facility, set_facility_sendto, addr, port);
}

syp_error reset_facility_udp (facility_t facility, const char * addr, uint16_t port)
{
  return send_uint32_by_function (facility, reset_facility_sendto, addr, port);
}

//-------------------------- DBUS -----------------------------------

/**
 * Connect to the DBUS bus and send a broadcast signal
 * TODO: non broadcasting signals
 */
syp_error dbus_sendsignal(const char * target UNUSED, char * signal_name, int value_type, void * signal_value)
{
  DBusMessage* msg;
  DBusMessageIter args;
  DBusConnection* conn;
  DBusError err;
  int ret;


  // initialise the error value
  dbus_error_init (&err);

  // connect to the DBUS system bus, and check for errors
  conn = dbus_bus_get (DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set (&err)) { 
    dbus_error_free (&err); 
  }
  if (NULL == conn) { 
    return ERR_DBUS;
  }

  // register our name on the bus, and check for errors
  ret = dbus_bus_request_name (conn, SYPLOG_DEFAULT_DBUS_SOURCE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
  if (dbus_error_is_set (&err)) { 
    dbus_error_free (&err); 
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
    return ERR_DBUS;
  }

  // create a signal & check for errors 
  msg = dbus_message_new_signal (SYPLOG_DEFAULT_DBUS_OBJECT, // object name of the signal
                                 SYPLOG_DBUS_INTERFACE, // interface name of the signal
                                 signal_name); // name of the signal
  if (NULL == msg) 
  { 
    return ERR_DBUS;
  }

   // append arguments onto signal
  dbus_message_iter_init_append (msg, &args);
  if (!dbus_message_iter_append_basic (&args, value_type, signal_value)) {
    return ERR_DBUS;
  }

  // send the message and flush the connection
  if (!dbus_connection_send (conn, msg, NULL)) {
    return ERR_DBUS;
  }
  dbus_connection_flush (conn);
  
  // free the message 
  dbus_message_unref(msg);

  return NOERR;
}

/**
 * Call a method on a remote object
 */
void * dbus_query(const char * target_name, char * method_name, int arg_type, void * method_arg ) 
{
  DBusMessage* msg;
  DBusMessageIter args;
  DBusConnection* conn;
  DBusError err;
  DBusPendingCall* pending;
  int ret;
  int try = 0;
  char * response = NULL;

  if (target_name == NULL)
    target_name = SYPLOG_DEFAULT_DBUS_TARGET;

  // initialiset the errors
  dbus_error_init(&err);

  // connect to the system bus and check for errors
  conn = dbus_bus_get (DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set (&err)) { 
    dbus_error_free(&err);
  }
  if (NULL == conn) { 
    return NULL;
  }

  // request our name on the bus
  ret = dbus_bus_request_name (conn, SYPLOG_DEFAULT_DBUS_SOURCE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
  if (dbus_error_is_set (&err)) { 
     dbus_error_free (&err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
    return NULL;
  }

  // create a new method call and check for errors
  msg = dbus_message_new_method_call (target_name, // target for the method call
                                      SYPLOG_DEFAULT_DBUS_OBJECT, // object to call on
                                      SYPLOG_DBUS_INTERFACE, // interface to call on
                                      method_name); // method name
  if (NULL == msg) { 
    return NULL;
  }

  // append arguments
  dbus_message_iter_init_append(msg, &args);
  if (!dbus_message_iter_append_basic (&args, arg_type, &method_arg)) {
    return NULL;
  }

  // send message and get a handle for a reply
  if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
    return NULL;
  }
  
  if (NULL == pending) { 
    return NULL;
  }
  dbus_connection_flush (conn);
  
  
  // free message
  dbus_message_unref (msg);
  
  // block until we recieve a reply
  dbus_pending_call_block (pending);

  // get the reply message
  for (try = 1; !dbus_pending_call_get_completed (pending) && try < 3; try++) {
    sleep (1);
  }

  if (!dbus_pending_call_get_completed (pending)) {
     dbus_pending_call_cancel (pending);
     return NULL;
  }

  msg = dbus_pending_call_steal_reply (pending);
  if (NULL == msg) {
    return NULL;
  }
  // free the pending message handle
  dbus_pending_call_unref (pending);

  // read the parameters
  if (dbus_message_iter_init(msg, &args))
    dbus_message_iter_get_basic(&args, &response);

  // free reply 
  dbus_message_unref(msg);

  return response;
}

#define PING_STR		"ping"

syp_error ping_syplog_dbus (const char * logger_name)
{
  const char * data = dbus_query (logger_name, SYPLOG_MESSAGE_PING_NAME, SYPLOG_PING_DBUS_TYPE, PING_STR);
  if (data == NULL)
    return ERR_DBUS;
  if (strncmp (data, PING_STR, 4) == 0)
  {
    return NOERR;
  }

  return ERR_DBUS;
}

syp_error set_level_dbus (log_level_t level, const char * logger_name)
{
  return dbus_sendsignal (logger_name, SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME,
                          SYPLOG_LOG_LEVEL_DBUS_TYPE, &level);
}

syp_error set_facility_dbus (facility_t facility, const char * logger_name)
{
  return dbus_sendsignal (logger_name, SYPLOG_SIGNAL_SET_FACILITY_NAME,
                          SYPLOG_FACILITY_DBUS_TYPE, &facility);
}

syp_error reset_facility_dbus (facility_t facility, const char * logger_name)
{
  return dbus_sendsignal (logger_name, SYPLOG_SIGNAL_RESET_FACILITY_NAME,
                          SYPLOG_FACILITY_DBUS_TYPE, &facility);
}
