/*! \file
    \brief Network protocol for controlling logger remotely. (implementation)
    \see control-protocol.h
*/

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
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#undef _GNU_SOURCE

#include "syp-error.h"
#include "control-protocol.h"

syp_error send_message_to (int socket, const void * message, size_t message_len, 
  const struct sockaddr * to, socklen_t tolen)
{
  ssize_t bytes_send = 0;
#ifdef ENABLE_CHECKING
  if (message == NULL)
    return ERR_BAD_PARAMS;
#endif
    
  bytes_send = sendto (socket, message, message_len, 0, to, tolen);
  if (bytes_send == -1)
  {
    return sys_to_syp_error (errno);
  }
  if (bytes_send < message_len)
    return ERR_TRUNCATED;
    
   return NOERR;
}


syp_error receive_message_from (int socket, void * message, ssize_t * message_len,
  struct sockaddr *from, socklen_t *fromlen)
{
  ssize_t bytes_received = 0;
#ifdef ENABLE_CHECKING
  if (message == NULL || message_len == NULL)
    return ERR_BAD_PARAMS;
#endif
    
  bytes_received = recvfrom (socket, message, *message_len, 0, from, fromlen);
  if (bytes_received == -1)
  {
    return sys_to_syp_error (errno);
  }
  
  *message_len = bytes_received;
    
   return NOERR;
}

syp_error send_uint32_action_to (int socket, message_type type, uint32_t data,
  const struct sockaddr * to, socklen_t tolen)
{
  uint32_t message[2]; // assume message_type + uint32_t

  message[0] = htonl(type);
  message[1] = htonl(data);
  
  return send_message_to (socket, (void*)message, sizeof (uint32_t) * 2,
    to, tolen);
}

syp_error receive_uint32_action_from (int socket, message_type * type, 
  uint32_t * data, struct sockaddr *from, socklen_t *fromlen)
{
  uint32_t message[2]; // assume message_type + uint32_t
  ssize_t chars_read = sizeof (uint32_t) * 2;
  syp_error ret_code = NOERR;
  
  ret_code =  receive_message_from (socket, (void*)message, &chars_read, from, fromlen);
  if (ret_code != NOERR)
    return ret_code;
    
  if (chars_read != sizeof (int32_t) + sizeof (uint32_t) )
    return ERR_TRUNCATED;
  
  *type = ntohl (message[0]);
  
  *data = ntohl (message[1]);
  
  return NOERR;
}

syp_error receive_typed_uint32_action_from (int socket, message_type type,
  uint32_t * data, struct sockaddr *from, socklen_t *fromlen)
{
  syp_error ret_code = NOERR;
  message_type received_type = MESSAGE_PING;
  
  ret_code = receive_uint32_action_from (socket,&received_type, data, from, fromlen);
  if (ret_code != NOERR)
    return ret_code;
  
  if (received_type != type)
    return ERR_BAD_MESSAGE;
  
  return NOERR;
}


syp_error set_level_sendto (int socket, log_level_t level, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (socket, MESSAGE_SET_LEVEL, level, to, tolen);
}

syp_error set_level_receive_from (int socket, log_level_t * level,
  struct sockaddr *from, socklen_t *fromlen)
{
  return receive_typed_uint32_action_from (socket, MESSAGE_SET_LEVEL, level,
    from, fromlen);
}

syp_error set_facility_sendto (int socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (socket, MESSAGE_SET_FACILITY, facility, to, tolen);
}

syp_error set_facility_receive_from (int socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen)
{
  return receive_typed_uint32_action_from (socket, MESSAGE_SET_FACILITY, facility,
    from, fromlen);
}

syp_error reset_facility_sendto (int socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen)
{
  return send_uint32_action_to (socket, MESSAGE_RESET_FACILITY, facility, to, tolen);
}

syp_error reset_facility_receive_from (int socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen)
{
  return receive_typed_uint32_action_from (socket, MESSAGE_RESET_FACILITY,
    facility, from, fromlen);
}
