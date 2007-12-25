/*! \file
    \brief Logger remote control listening implementation.
    \see listener.h
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
#include <unistd.h>
#include <errno.h>
#undef _GNU_SOURCE

#include "listener.h"
#include "control-protocol.h"

syp_error handle_ping (listener controller)
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

syp_error handle_set_level (listener controller)
{
  log_level_t new_level = LOG_ALL;
  syp_error ret_code = NOERR;
  ret_code = set_level_receive (controller->socket, &new_level);
  if (ret_code == NOERR)
    ret_code = set_log_level (controller->target, new_level);
  
  return ret_code;
}

syp_error handle_set_facility (listener controller)
{
  facility_t new_facility = FACILITY_ALL;
  syp_error ret_code = NOERR;
  ret_code = set_facility_receive (controller->socket, &new_facility);
  if (ret_code == NOERR)
    ret_code = set_facility (controller->target, new_facility);
  
  return ret_code;
}

syp_error handle_reset_facility (listener controller)
{
  facility_t new_facility = FACILITY_ALL;
  syp_error ret_code = NOERR;
  ret_code = reset_facility_receive (controller->socket, &new_facility);
  if (ret_code == NOERR)
    ret_code = reset_facility (controller->target, new_facility);
  
  return ret_code;
}

syp_error handle_invalid_message (listener controller)
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

void * listen_loop (void * data)
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
    
    bytes_read = recv (controller->socket, &next_message, sizeof (message_type),
      MSG_PEEK);
    if (bytes_read != sizeof (message_type))
    {
      handle_invalid_message (controller);
    }
    
    switch (ntohl(next_message))
    {
      case MESSAGE_PING:
        status = handle_ping(controller);
        break;
      case MESSAGE_SET_LEVEL:
        status = handle_set_level(controller);
        break;
      case MESSAGE_SET_FACILITY:
        status = handle_set_facility(controller);
        break;
      case MESSAGE_RESET_FACILITY:
        status = handle_reset_facility(controller);
        break;
      default:
        handle_invalid_message (controller);
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
    listen_loop, (void*)controller);
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
syp_error start_listen_unix (listener controller, logger target, const char * socket_name)
{
  return ERR_NOT_IMPLEMENTED;
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
  
  close (controller->socket);
  controller->socket = -1;
  id = controller->thread_id;
  sys_ret = pthread_cancel (id);
  if (sys_ret != NOERR)
  {
    ret_code = sys_to_syp_error (sys_ret);
    goto FINISHING;
  }

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
