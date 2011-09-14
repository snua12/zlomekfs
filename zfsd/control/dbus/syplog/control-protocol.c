/*! \file
    \brief Network protocol for controlling logger remotely. (implementation)
    \see control-protocol.h
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
#include <errno.h>

#include "syp-error.h"
#include "control-protocol.h"

/** send raw message to net socket
 * @param msg_socket socket file descriptor. Socket must be initialized.
 * @param message pointer to raw message data in network byte order.
 * @param message_len message length in bytes
 * @param to in case of statefull protocols (tcp) NULL, 
    in case of stateless protocols (udp) valid pointer to receiver's address
 * @param tolen length of to (sizeof in bytes)
 * @return ERR_BAD_PARAMS, ERR_TRUNCATED, system related errors
*/
static syp_error send_message_to (int msg_socket, const void * message, size_t message_len, 
  const struct sockaddr * to, socklen_t tolen)
{
  ssize_t bytes_send = 0;
#ifdef ENABLE_CHECKING
  if (message == NULL)
    return ERR_BAD_PARAMS;
#endif
    
  bytes_send = sendto (msg_socket, message, message_len, 0, to, tolen);
  if (bytes_send == -1)
  {
    return sys_to_syp_error (errno);
  }
  if ((size_t) bytes_send < message_len)
    return ERR_TRUNCATED;
    
   return NOERR;
}

/** receive raw message from net socket (blocking call)
 * @param msg_socket socket file descriptor. Socket must be initialized.
 * @param message pointer to buffer, where to store received data.
    Data will be stored in network byte order.
 * @param message_len message buffer length in bytes (maximum bytes to receive).
    Upon successfull receivement the number of bytes received will be stored here.
 * @param from if not NULL and underlying protocol provides source address,
    the sender's address will be stored here
 * @param fromlen if not NULL, the length of from (size in bytes) will be stored here.
    Must be NULL if from is NULL and vice versa.
 * @return ERR_BAD_PARAMS, ERR_TRUNCATED, system related errors
*/
static syp_error receive_message_from (int msg_socket, void * message, ssize_t * message_len,
  struct sockaddr *from, socklen_t *fromlen)
{
  ssize_t bytes_received = 0;
#ifdef ENABLE_CHECKING
  if (message == NULL || message_len == NULL)
    return ERR_BAD_PARAMS;
#endif
    
  bytes_received = recvfrom (msg_socket, message, *message_len, 0, from, fromlen);
  if (bytes_received == -1)
  {
    return sys_to_syp_error (errno);
  }
  
  *message_len = bytes_received;
    
   return NOERR;
}

/** Format message and send it.
 * Params msg_socket, to, tolen have the same meaning as in function send_message_to
 * @see send_message_to
 * @param type command enum (what to do). In local byte order.
 * @see message_type
 * @param data data for action - log level for set_log_level, 
    facilities for set_facilities, etc. In local byte order.
 * @return the same errors as send_message_to
*/
static syp_error send_uint32_action_to (int msg_socket, message_type type, uint32_t data,
  const struct sockaddr * to, socklen_t tolen)
{
  uint32_t message[2]; // assume message_type + uint32_t

  message[0] = htonl(type);
  message[1] = htonl(data);
  
  return send_message_to (msg_socket, (void*)message, sizeof (uint32_t) * 2,
    to, tolen);
}

/** Receive message from socket and parse it.
 * Params msg_socket, from and fromlen have the same meaning as in function receive_message_from
 * @see receive_message_from
 * @param type Valid pointer, command enum (what to do) in local byte order will be set.
 * @see message_type
 * @param data Valid pointer, data for action - log level for set_log_level,
    facilities for set_facilities, etc in local byte order will be filled.
 * @return the same errors as send_message_to 
    + ERR_TRUNCATED in case of short message
*/
static syp_error receive_uint32_action_from (int msg_socket, message_type * type, 
  uint32_t * data, struct sockaddr *from, socklen_t *fromlen)
{
  uint32_t message[2]; // assume message_type + uint32_t
  ssize_t chars_read = sizeof (uint32_t) * 2;
  syp_error ret_code = NOERR;
  
  ret_code =  receive_message_from (msg_socket, (void*)message, &chars_read, from, fromlen);
  if (ret_code != NOERR)
    return ret_code;
    
  if (chars_read != sizeof (int32_t) + sizeof (uint32_t) )
    return ERR_TRUNCATED;
  
  *type = ntohl (message[0]);
  
  *data = ntohl (message[1]);
  
  return NOERR;
}

/** Receive first message from socket (discards) and checks if type is correct.
 * params msg_socket, data, from and fromlen have the same meaning as receive_uint32_action_from
 * @see receive_uint32_action_from
 * @param type required type to receive. If the received message doesn't have this type,
    ERR_BAD_MESSAGE will be returned.
 * @return the same errors as receive_uint32_action_from 
    + ERR_BAD_MESSAGE in case of type mismatch
 */
static syp_error receive_typed_uint32_action_from (int msg_socket, message_type type,
  uint32_t * data, struct sockaddr *from, socklen_t *fromlen)
{
  syp_error ret_code = NOERR;
  message_type received_type = MESSAGE_PING;
  
  ret_code = receive_uint32_action_from (msg_socket,&received_type, data, from, fromlen);
  if (ret_code != NOERR)
    return ret_code;
  
  if (received_type != type)
    return ERR_BAD_MESSAGE;
  
  return NOERR;
}

syp_error set_level_sendto (int msg_socket, log_level_t level, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (msg_socket, MESSAGE_SET_LEVEL, level, to, tolen);
}

syp_error set_level_receive_from (int msg_socket, log_level_t * level,
  struct sockaddr *from, socklen_t *fromlen)
{
  return receive_typed_uint32_action_from (msg_socket, MESSAGE_SET_LEVEL, level,
    from, fromlen);
}

syp_error set_facility_sendto (int msg_socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (msg_socket, MESSAGE_SET_FACILITY, facility, to, tolen);
}

syp_error set_facility_receive_from (int msg_socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen)
{
  return receive_typed_uint32_action_from (msg_socket, MESSAGE_SET_FACILITY, facility,
    from, fromlen);
}

syp_error reset_facility_sendto (int msg_socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (msg_socket, MESSAGE_RESET_FACILITY, facility, to, tolen);
}

syp_error reset_facility_receive_from (int msg_socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen)
{
  return receive_typed_uint32_action_from (msg_socket, MESSAGE_RESET_FACILITY,
    facility, from, fromlen);
}
