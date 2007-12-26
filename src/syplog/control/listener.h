#ifndef	LISTENER_H
#define	LISTENER_H
/*! \file
    \brief Logger remote control listening interface.

    When logger needs to be remotely controlled, it is needed to
    start listener on it. Listener then receives messages and 
    change log level and facilities.

    Simply call start_listen_udp (providing logger to be controlled)
    to start listening loop (will fork separate thread),
    call stop_listen to close listening socket and stop listening thread.
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

#include "syp-error.h"
#include "syplog.h"
#include "control-protocol.h"

/// structure with configuration and state of listener
typedef struct listener_def
{
  /// logger which should receive configuration changes
  logger target;
  /// type of communication (udp, unix socket, etc)
  communication_type type;
  union {
    /// port on which to listen in upd mode
    uint16_t port;
    /// unix socket name
    char socket_name[FILE_NAME_LEN];
  };
  /// socket file descriptor 
  int socket;
  /// thread id which performs listening loops
  pthread_t thread_id;
  /// mutex for this structure
  pthread_mutex_t mutex;
} * listener;


/** start listening on upd port
 *
 * @param controller pointer to uninitalized structure of listener which will hold the state
 * @param target logger to control
 * @param port port number to listen on
 */
syp_error start_listen_udp (listener controller, logger target, uint16_t port);

/** start listening on socket name
 *
 * @param controller pointer to uninitalized structure of listener which will hold the state
 * @param target logger to control
 * @param socket_name file name of unix socket to listen on
 */
syp_error start_listen_unix (listener controller, logger target, const char * socket_name);

/** stop control listening
 * 
 * @param controller valid pointer to running listener structure
 */
syp_error stop_listen (listener controller);

#endif	/* LISTENER_H */
