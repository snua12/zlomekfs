/*! \file
    \brief Logger remote control listening implementation.
    \see listener.h
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
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#undef _GNU_SOURCE

#include "listener.h"
#include "control-protocol.h"



//------------------------------- UDP -----------------------------------

/** handle ping message
 * @param controller initialized listener with ping message on top of net stack.
 * @return NOERR;
 */
syp_error handle_socket_ping (listener controller)
{
  struct sockaddr from;
  socklen_t fromlen = 0;
  char buffer[1024];
  ssize_t received = 0;
  
  memset (&from, 0, sizeof (struct sockaddr));
  
  received = recvfrom (controller->socket, buffer, 1024, 0, &from, &fromlen);
  
  sendto (controller->socket, buffer, received, 0, &from, fromlen);
  
  return NOERR;
}

/** handle set_log_level message (receive log level and set it to logger)
 * @param controller initialized listener with set_log_level message on top of net stack.
 * @see set_level_receive
 * @see set_log_level
 * @return the same as set_level_receive and set_log_level
 */
syp_error handle_socket_set_level (listener controller)
{
  log_level_t new_level = LOG_ALL;
  syp_error ret_code = NOERR;
  ret_code = set_level_receive (controller->socket, &new_level);
  if (ret_code == NOERR)
    ret_code = set_log_level (controller->target, new_level);
  
  return ret_code;
}

/** handle set_facility message (receive facility and set it as to be logged)
 * @param controller initialized listener with set_facility message on top of net stack.
 * @see set_facility_receive
 * @see set_facility
 * @return the same as set_facility_receive and set_facility
 */
syp_error handle_socket_set_facility (listener controller)
{
  facility_t new_facility = FACILITY_ALL;
  syp_error ret_code = NOERR;
  ret_code = set_facility_receive (controller->socket, &new_facility);
  if (ret_code == NOERR)
    ret_code = set_facility (controller->target, new_facility);
  
  return ret_code;
}

/** handle reset_facility message (receive facility and set it as not to be logged)
 * @param controller initialized listener with reset_facility message on top of net stack.
 * @see reset_facility_receive
 * @see reset_facility
 * @return the same as reset_facility_receive and reset_facility
 */
syp_error handle_socket_reset_facility (listener controller)
{
  facility_t new_facility = FACILITY_ALL;
  syp_error ret_code = NOERR;
  ret_code = reset_facility_receive (controller->socket, &new_facility);
  if (ret_code == NOERR)
    ret_code = reset_facility (controller->target, new_facility);
  
  return ret_code;
}

/** handle unknown message (discard it from net stack and report it)
 * @param controller initialized listener unempty net stack
 * @return NOERR
 */
syp_error handle_socket_invalid_message (listener controller)
{  
  ssize_t bytes_read = 0;
  char wrong_message_buffer[1024] = "";

  bytes_read = recv(controller->socket, wrong_message_buffer, 1023, 0);
  if (bytes_read >= 0)
    wrong_message_buffer[bytes_read] = '\0';
  else
    wrong_message_buffer[0] = '\0';
  do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
    "Log controller has received corrupted data '%s'\n", wrong_message_buffer);
  
  return NOERR;
}

/** Listening thread main loop function.
 * Check for messages and receive them
 * @param data pointer to initialized listener
 * @return NULL
 */
void * socket_listen_loop (void * data)
{
  syp_error status = NOERR;
  listener controller = (listener) data;
  message_type next_message = MESSAGE_PING;
  ssize_t bytes_read = 0;
  
  while (status == NOERR)
  {
    pthread_mutex_lock (&(controller->mutex));
    
    if (controller->socket == -1)
    {
      status = ERR_NOT_INITIALIZED;
      goto NEXT;
    }
    // TODO: use non-blocking variant with timeout
    bytes_read = recv (controller->socket, &next_message, sizeof (message_type),
      MSG_PEEK);
    if (bytes_read != sizeof (message_type))
    {
      handle_socket_invalid_message (controller);
    }
    
    switch (ntohl(next_message))
    {
      case MESSAGE_PING:
        status = handle_socket_ping(controller);
        break;
      case MESSAGE_SET_LEVEL:
        status = handle_socket_set_level(controller);
        break;
      case MESSAGE_SET_FACILITY:
        status = handle_socket_set_facility(controller);
        break;
      case MESSAGE_RESET_FACILITY:
        status = handle_socket_reset_facility(controller);
        break;
      default:
        handle_socket_invalid_message (controller);
        break;
    }
    if (status != NOERR)
    {
      do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
        "Log controller has received unknown action '%d'\n", next_message);
      status = NOERR;
    }

NEXT:
    pthread_mutex_unlock (&(controller->mutex));
  }
  
  return NULL;
}

syp_error start_listen_udp (listener controller, logger target, uint16_t port)
{
  int sys_ret = 0;
  syp_error ret_code = NOERR;
  struct sockaddr_in addr;
  
#ifdef	ENABLE_CHECKING
  if (controller == NULL || target == NULL || port == 0)
    return ERR_BAD_PARAMS;
#endif
  
  // init structure
  memset (controller, 0, sizeof (struct listener_def));
  sys_ret = pthread_mutex_init(&(controller->mutex), NULL);
  if (sys_ret != 0)
    return sys_to_syp_error(sys_ret);

  controller->target = target;
  controller->type = COMM_UDP;
  controller->port = port;

  
  // lock
  pthread_mutex_lock (&(controller->mutex));
  
  // init socket
  controller->socket = socket(AF_INET,SOCK_DGRAM,0);
  if (controller->socket == -1)
  {
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  
  sys_ret = bind (controller->socket, (const struct sockaddr *)&addr,
    sizeof (struct sockaddr_in));
  if (sys_ret == -1)
  {
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }

  sys_ret = pthread_create (&(controller->thread_id), NULL,
    socket_listen_loop, (void*)controller);
  if (sys_ret != 0)
  {
    ret_code = sys_to_syp_error (sys_ret);
    goto FINISHING;
  }
  
FINISHING:
  pthread_mutex_unlock ((&controller->mutex));
  if (ret_code != NOERR)
    pthread_mutex_destroy(&(controller->mutex));
  
  return ret_code;
}


//------------------------ DBUS ----------------------------------

void dbus_reply_to_ping( listener controller, DBusMessage* msg, DBusConnection* conn)
{
  DBusMessage* reply = NULL;
  DBusMessageIter args;
  const char* param = NULL;

  // read the arguments
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Syplog ping without arg\n");
    param = "";
  }
  else if (SYPLOG_PING_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Wrong argument type to syplog ping\n");
  else 
    dbus_message_iter_get_basic (&args, &param);

  do_log (controller->target, LOG_DEBUG, FACILITY_LOG, 
          "ping called with %s\n", param);
  // create a reply from the message
  reply = dbus_message_new_method_return (msg);
  if (reply == NULL)
    goto FINISHING;

  // add the arguments to the reply
  dbus_message_iter_init_append (reply, &args);
  if (!dbus_message_iter_append_basic (&args, SYPLOG_PING_DBUS_TYPE, &param)) 
  { 
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Out of memory in sending reply to ping\n");
    goto FINISHING;
  }

  // send the reply && flush the connection
  if (!dbus_connection_send (conn, reply, NULL)) {
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Out of memory in sending reply to ping\n");
    goto FINISHING;
  }
  dbus_connection_flush (conn);

FINISHING:
  // free the reply
  if (reply != NULL)
    dbus_message_unref (reply);
}

void handle_dbus_reset_facility (listener controller, DBusMessage* msg) {
  DBusMessageIter args;
  facility_t facility = FACILITY_NOTHING;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for reset_facility \n");
  }
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
  {
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for reset facility\n");
  }
  else 
  {
    dbus_message_iter_get_basic (&args, &facility);
    do_log (controller->target, LOG_DATA, FACILITY_LOG, 
            "Got reset facility with value %d\n", facility);
    reset_facility (controller->target, facility);
  }
}

void handle_dbus_set_facility (listener controller, DBusMessage* msg) {
  DBusMessageIter args;
  facility_t facility = FACILITY_NOTHING;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for set_facility \n");
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for set facility\n");
  else {
    dbus_message_iter_get_basic (&args, &facility);
    do_log (controller->target, LOG_DATA, FACILITY_LOG, 
            "Got set facility with value %d\n", facility);
    set_facility (controller->target, facility);
  }
}

void handle_dbus_set_log_level(listener controller, DBusMessage* msg) {
  DBusMessageIter args;
  log_level_t level = LOG_NONE;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for set level \n");
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
    do_log (controller->target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for set level\n");
  else {
    dbus_message_iter_get_basic (&args, &level);
    do_log (controller->target, LOG_DATA, FACILITY_LOG, 
            "Got set level with value %d\n", level);
    set_log_level (controller->target, level);
  }
}

void * dbus_listen_loop (void * data)
{
  syp_error status = NOERR;
  listener controller = (listener) data;
  DBusMessage* msg;
  
  while (status == NOERR)
  {
    pthread_mutex_lock (&(controller->mutex));
    
    if (controller->dbus_conn == NULL)
    {
      status = ERR_NOT_INITIALIZED;
      goto NEXT;
    }

    // non blocking read of the next available message
    dbus_connection_read_write(controller->dbus_conn, DBUS_WAIT_TIMEOUT);
    msg = dbus_connection_pop_message(controller->dbus_conn);

    // loop again if we haven't got a message
    if (NULL == msg) { 
      sleep (DBUS_WAIT_TIMEOUT / 1000);

      goto NEXT;
    }
    do_log (controller->target, LOG_DEBUG, FACILITY_DBUS, "we got a message\n");  
    // check what signal or message this is
    if (dbus_message_is_method_call(msg, SYPLOG_DBUS_INTERFACE, SYPLOG_MESSAGE_PING_NAME)) 
      dbus_reply_to_ping(controller, msg, controller->dbus_conn);

    if (dbus_message_is_signal(msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME))
      handle_dbus_set_log_level(controller, msg);

    if (dbus_message_is_signal(msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_SET_FACILITY_NAME))
      handle_dbus_set_facility(controller, msg);

    if (dbus_message_is_signal(msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_RESET_FACILITY_NAME))
      handle_dbus_reset_facility(controller, msg);

    // free the message
    dbus_message_unref(msg);

NEXT:
    pthread_mutex_unlock (&(controller->mutex));
  }

  dbus_bus_release_name (controller->dbus_conn, SYPLOG_DEFAULT_DBUS_TARGET, NULL);
  dbus_connection_unref (controller->dbus_conn);
  
  return NULL;
} //TODO: implement this

syp_error start_listen_dbus (listener controller, logger target, const char * name)
{
  syp_error ret_code = NOERR;
  int sys_ret = 0;
  
#ifdef	ENABLE_CHECKING
  if (controller == NULL || target == NULL)
    return ERR_BAD_PARAMS;
#endif

  if (name == NULL)
    name = SYPLOG_DEFAULT_DBUS_TARGET;

  // init structure
  memset (controller, 0, sizeof (struct listener_def));
  sys_ret = pthread_mutex_init(&(controller->mutex), NULL);
  if (sys_ret != 0)
    return sys_to_syp_error(sys_ret);

  // lock
  pthread_mutex_lock (&(controller->mutex));
  controller->target = target;
  controller->type = COMM_DBUS;


  
  dbus_error_init(&(controller->dbus_err));

  // connect to the bus and check for errors
  controller->dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &(controller->dbus_err));
  if (dbus_error_is_set(&(controller->dbus_err))) { 
    do_log (target, LOG_ERROR, FACILITY_LOG, "Connection Error (%s)\n", 
            controller->dbus_err.message);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }
  if (NULL == controller->dbus_conn) {
    do_log (target, LOG_ERROR, FACILITY_LOG, "Got NULL connection from dbus\n");
    ret_code = ERR_DBUS;
    goto FINISHING;
  }

  // request our name on the bus and check for errors
  sys_ret = dbus_bus_request_name(controller->dbus_conn, name,
                              DBUS_NAME_FLAG_REPLACE_EXISTING, 
                              &(controller->dbus_err));
  if (dbus_error_is_set(&(controller->dbus_err))) { 
    do_log (target, LOG_ERROR, FACILITY_LOG, "Dbus name Error (%s)\n", 
            controller->dbus_err.message); 
    ret_code = ERR_DBUS;
    goto FINISHING;
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != sys_ret) { 
    do_log (target, LOG_ERROR, FACILITY_LOG, "Dbus not Primary Owner (%d)\n", sys_ret);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }


  // add a rule for which messages we want to see
  dbus_bus_add_match(controller->dbus_conn,
                     "type='signal',interface='" SYPLOG_DBUS_INTERFACE "'",
                     &(controller->dbus_err)); // see signals from the given interface
  dbus_connection_flush(controller->dbus_conn);
  if (dbus_error_is_set(&(controller->dbus_err))) { 
    do_log (target, LOG_ERROR, FACILITY_LOG, "Can't register dbus signal match (%s)\n",
            controller->dbus_err.message);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }

  sys_ret = pthread_create (&(controller->thread_id), NULL,
    dbus_listen_loop, (void*)controller);
  if (sys_ret != 0)
  {
    ret_code = sys_to_syp_error (sys_ret);
    goto FINISHING;
  }
  
FINISHING:
  pthread_mutex_unlock ((&controller->mutex));
  if (ret_code != NOERR)
  {
    dbus_error_free(&(controller->dbus_err));

    pthread_mutex_destroy(&(controller->mutex));
  }
  
  return ret_code;
}

syp_error stop_listen_udp(listener controller)
{
  close (controller->socket);
  controller->socket = -1;

  return NOERR;
}

syp_error stop_listen_dbus(listener controller)
{
  dbus_error_free(&(controller->dbus_err));

  return NOERR;
}

syp_error stop_listen (listener controller)
{
  syp_error ret_code = NOERR;
  int sys_ret = 0;
  pthread_t id = 0;
  
#ifdef ENABLE_CHECKING
  if (controller == NULL)
    return ERR_BAD_PARAMS;
#endif

  pthread_mutex_lock (&(controller->mutex));
  

  switch (controller->type)
  {
    case COMM_UDP:
      ret_code = stop_listen_udp (controller);
      break;
    case COMM_DBUS:
      ret_code = stop_listen_dbus (controller);
      break;
    default:
      ret_code = ERR_BAD_PARAMS;
      break;
  }
  if (ret_code != NOERR)
    goto FINISHING;

  // give the thread chance to exit normally
  pthread_mutex_unlock (&(controller->mutex));
  sleep (DBUS_WAIT_TIMEOUT * 2);
  pthread_mutex_lock (&(controller->mutex));

  id = controller->thread_id;
  pthread_cancel (id);

  sys_ret = pthread_join (id, NULL);
  if (sys_ret != NOERR)
  {
    ret_code = sys_to_syp_error (sys_ret);
    goto FINISHING;
  }

  pthread_mutex_unlock (&(controller->mutex));
  pthread_mutex_destroy (&(controller->mutex));
  
FINISHING:

  return ret_code;
}
