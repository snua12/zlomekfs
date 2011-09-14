/*! \file
    \brief Logger remote control listening implementation.
    \see listener.h
*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Syplog; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/


#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "listener.h"
#include "control-protocol.h"



//------------------------------- UDP -----------------------------------

/** handle ping message
 * @param controller initialized listener with ping message on top of net stack.
 * @return NOERR;
 */
static syp_error handle_socket_ping (listener controller)
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
static syp_error handle_socket_set_level (listener controller)
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
static syp_error handle_socket_set_facility (listener controller)
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
static syp_error handle_socket_reset_facility (listener controller)
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
static syp_error handle_socket_invalid_message (listener controller)
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
static void * socket_listen_loop (void * data)
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


/** reply to ping message received through dbus
 *
 * @param target initialized logger where to put error messages
 * @param msg received message of type SYPLOG_MESSAGE_PING_NAME
 * @param conn connection to which send the reply
 * @return std errors
 */
static syp_error dbus_reply_to_ping( logger target, DBusMessage* msg, DBusConnection* conn)
{
  DBusMessage * reply = NULL;
  DBusMessageIter args;
  const char * param = NULL;
  syp_error ret_code = NOERR;

  // read the arguments
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Syplog ping without arg\n");
    param = "";
  }
  else if (SYPLOG_PING_DBUS_TYPE != dbus_message_iter_get_arg_type(&args))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Wrong argument type to syplog ping\n");
    ret_code = ERR_DBUS;
    param = "ping";
  }
  else 
    dbus_message_iter_get_basic (&args, &param);

  do_log (target, LOG_DEBUG, FACILITY_LOG, 
          "ping called with %s\n", param);
  // create a reply from the message
  reply = dbus_message_new_method_return (msg);
  if (reply == NULL)
  {
    ret_code = ERR_DBUS;
    goto FINISHING;
  }

  // add the arguments to the reply
  dbus_message_iter_init_append (reply, &args);
  if (!dbus_message_iter_append_basic (&args, SYPLOG_PING_DBUS_TYPE, &param)) 
  { 
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Out of memory in sending reply to ping\n");
    ret_code = ERR_NO_MEMORY;
    goto FINISHING;
  }

  // send the reply && flush the connection
  if (!dbus_connection_send (conn, reply, NULL))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Error when sending reply to ping\n");
    ret_code = ERR_DBUS;
    goto FINISHING;
  }
  dbus_connection_flush (conn);

FINISHING:
  // free the reply
  if (reply != NULL)
    dbus_message_unref (reply);

  return ret_code;
}

/** handle dbus signal to reset facility
 *
 * @param target initialized logger where to reset facility
 * @param msg received message of type SYPLOG_SIGNAL_RESET_FACILITY_NAME
 * @return std errors
 */
static syp_error handle_dbus_reset_facility (logger target, DBusMessage* msg) {
  DBusMessageIter args;
  facility_t facility = FACILITY_NOTHING;
  syp_error ret_code = NOERR;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for reset_facility \n");
    ret_code = ERR_DBUS;
  }
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for reset facility\n");
    ret_code = ERR_DBUS;
  }
  else 
  {
    dbus_message_iter_get_basic (&args, &facility);
    do_log (target, LOG_DATA, FACILITY_LOG, 
            "Got reset facility with value %d\n", facility);
    ret_code = reset_facility (target, facility);
  }

  return ret_code;
}

/** handle dbus signal to set facility
 *
 * @param target initialized logger where to reset facility
 * @param msg received message of type SYPLOG_SIGNAL_SET_FACILITY_NAME
 * @return std errors
 */
static syp_error handle_dbus_set_facility (logger target, DBusMessage* msg) {
  DBusMessageIter args;
  facility_t facility = FACILITY_NOTHING;
  syp_error ret_code = NOERR;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for set_facility \n");
    ret_code = ERR_DBUS;
  }
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for set facility\n");
    ret_code = ERR_DBUS;
  }
  else
  {
    dbus_message_iter_get_basic (&args, &facility);
    do_log (target, LOG_DATA, FACILITY_LOG, 
            "Got set facility with value %d\n", facility);
    ret_code = set_facility (target, facility);
  }

  return ret_code;
}

/** handle dbus signal to set log level
 *
 * @param target initialized logger where to reset facility
 * @param msg received message of type SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME
 * @return std errors
 */
static syp_error handle_dbus_set_log_level (logger target, DBusMessage* msg) {
  DBusMessageIter args;
  log_level_t level = LOG_NONE;
  syp_error ret_code = NOERR;

  // read the parameters
  if (!dbus_message_iter_init (msg, &args))
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Can't get args for set level \n");
    ret_code = ERR_DBUS;
  }
  else if (SYPLOG_FACILITY_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
  {
    do_log (target, LOG_WARNING, FACILITY_LOG, 
            "Wrong arg type for set level\n");
    ret_code = ERR_DBUS;
  }
  else 
  {
    dbus_message_iter_get_basic (&args, &level);
    do_log (target, LOG_DATA, FACILITY_LOG, 
            "Got set level with value %d\n", level);
    ret_code = set_log_level (target, level);
  }

  return ret_code;
}

/// Register syplog names to dbus connection
 syp_error dbus_add_syplog_name (DBusConnection * connection, 
                                DBusError * err_struct, logger err_target)
{
  syp_error ret_code = NOERR;
  int sys_ret = 0;

  // request our name on the bus and check for errors
  sys_ret = dbus_bus_request_name(connection, SYPLOG_DEFAULT_DBUS_TARGET,
                              DBUS_NAME_FLAG_REPLACE_EXISTING, 
                              err_struct);
  if (dbus_error_is_set(err_struct)) { 
    if (err_target != NULL)
      do_log (err_target, LOG_ERROR, FACILITY_LOG, "Dbus name Error (%s)\n",
              err_struct->message);
    dbus_error_free (err_struct);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != sys_ret) { 
    if (err_target != NULL)
      do_log (err_target, LOG_ERROR, FACILITY_LOG, "Dbus not Primary Owner (%d)\n",
              sys_ret);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }


  // add a rule for which messages we want to see
  dbus_bus_add_match(connection,
                     SYPLOG_SIGNAL_RECEIVE_RULE,
                     err_struct); // see signals from the given interface
  dbus_connection_flush(connection);
  if (dbus_error_is_set(err_struct)) { 
    if (err_target != NULL)
      do_log (err_target, LOG_ERROR, FACILITY_LOG, 
              "Can't register dbus signal match (%s)\n",
              err_struct->message);
    dbus_error_free (err_struct);
    ret_code = ERR_DBUS;
    goto FINISHING;
  }

FINISHING:
  if (ret_code != NOERR)
  {
    // we should release name and match here
  }

  return ret_code;
}

/// Release syplog names from dbus connection
 syp_error dbus_release_syplog_name (DBusConnection * connection,
                                    DBusError * err_struct, logger err_target)
{
  syp_error ret_code = NOERR;
  dbus_bus_remove_match (connection, SYPLOG_SIGNAL_RECEIVE_RULE, err_struct);
  if (dbus_error_is_set(err_struct)) { 
    if (err_target != NULL)
      do_log (err_target, LOG_WARNING, FACILITY_LOG | FACILITY_DBUS, 
              "Can't unergister syplog dbus signal match (%s)\n",
              err_struct->message);
    dbus_error_free (err_struct);
    ret_code = ERR_DBUS;
  }
  
  dbus_bus_release_name (connection, SYPLOG_DEFAULT_DBUS_TARGET, err_struct);
  if (dbus_error_is_set(err_struct)) { 
    if (err_target != NULL)
      do_log (err_target, LOG_WARNING, FACILITY_LOG | FACILITY_DBUS,
              "Can't release syplog dbus name (%s)\n",
              err_struct->message);
    dbus_error_free (err_struct);
    ret_code = ERR_DBUS;
  }

  return ret_code;
}

/// Try to handle dbus message for syplog
syp_error dbus_handle_syplog_message (DBusConnection * conn, 
                                DBusError * err_struct UNUSED, DBusMessage * msg,
                                logger target)
{
  if (dbus_message_is_method_call (msg, SYPLOG_DBUS_INTERFACE, SYPLOG_MESSAGE_PING_NAME)) 
    return dbus_reply_to_ping (target, msg, conn);

  if (dbus_message_is_signal (msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME))
    return handle_dbus_set_log_level (target, msg);

  if (dbus_message_is_signal (msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_SET_FACILITY_NAME))
    return handle_dbus_set_facility (target, msg);

  if (dbus_message_is_signal (msg, SYPLOG_DBUS_INTERFACE, SYPLOG_SIGNAL_RESET_FACILITY_NAME))
    return handle_dbus_reset_facility (target, msg);

  return ERR_BAD_MESSAGE;
}

/** loops listening on dbus connection waiting for new messages
    and handle the syplog ones
    to stop the loop lock listener and NULL the dbus_conn pointer
 *
 * @param data pointer to struct listener_def
          initialized for usage with dbus (opened connection with names registered)
 * @return NULL
*/
static void * dbus_listen_loop (void * data)
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
    do_log (controller->target, LOG_DEBUG, FACILITY_DBUS | FACILITY_LOG,
            "we got a message\n");

    // check what signal or message this is and try to handle
    status = dbus_handle_syplog_message (controller->dbus_conn, &(controller->dbus_err),
                                         msg, controller->target);
    if (status == ERR_BAD_MESSAGE)
    {
      do_log (controller->target, LOG_ERROR, FACILITY_DBUS | FACILITY_LOG, 
              "Unknown message received by syplog dbus loop.");
    }
    else if (status != NOERR)
      do_log (controller->target, LOG_ERROR, FACILITY_DBUS | FACILITY_LOG, 
              "Unknown error when handling dbus message %d: %s", status,
              syp_error_to_string (status));

    status = NOERR;
    // free the message
    dbus_message_unref(msg);

NEXT:
    pthread_mutex_unlock (&(controller->mutex));
  }
  
  return NULL;
} //TODO: implement this

syp_error start_listen_dbus (listener controller, logger target, 
                             const char * name UNUSED)
{
  syp_error ret_code = NOERR;
  int sys_ret = 0;
  
#ifdef	ENABLE_CHECKING
  if (controller == NULL || target == NULL)
    return ERR_BAD_PARAMS;
#endif

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


  ret_code = dbus_add_syplog_name (controller->dbus_conn, &(controller->dbus_err), controller->target);
  if (ret_code != NOERR)
    goto FINISHING;


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


/** Send signal to udp listening thread to terminate
 *
 * @param controller initialized listener listening on udp socket
 * @return NOERR
*/
static syp_error stop_listen_udp(listener controller)
{
  close (controller->socket);
  controller->socket = -1;

  return NOERR;
}

/** Send signal to dbus listening thread to terminate
 *
 * @param controller initialized listener listening on dbus
 * @return NOERR
*/
static syp_error stop_listen_dbus(listener controller)
{
  dbus_release_syplog_name (controller->dbus_conn, &(controller->dbus_err), 
                            controller->target);
  dbus_connection_unref (controller->dbus_conn);
  dbus_error_free(&(controller->dbus_err));
  controller->dbus_conn = NULL;

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
