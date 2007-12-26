/*! \file
    \brief Interface to send commands to logger (client side).
    \see control-protocol.h

    Use set_level_udp, set_facility_upd or reset_facility_udp to control
    behaviour of remote log.
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

#ifndef CONTROL_H
#define	CONTROL_H

#include "control-protocol.h"
#include "log-constants.h"

/** Send logger action message by udp using provided function.
  Compose destination address, bind socket and call provided function.
 * @param data data to send (log level, facility, etc)
 * @param function function to use to send data. 
    function has to be typed (set_(level|facility|...)_,
    gets socket fd, uint32_t data and destination address (must be valid)
 * @param ip ip address of receiver 
    (if NULL, DEFAULT_COMMUNICATION_ADDRESS will be used)
 * @param port port on receiver's side
    (if 0, DEFAULT_COMMUNICATION_PORT will be used)
 * @return system errors and errors returned by function
*/
syp_error send_uint32_by_function (uint32_t data, 
  syp_error (*function)(int, uint32_t, const struct sockaddr *, socklen_t), 
  const char * ip, uint16_t port);

/** Send command to set log_level to level to logger listening on addr:port address
 * @see send_uint32_by_function
 * @see set_log_level
 */
#define set_level_udp(level,addr,port)	send_uint32_by_function(level,set_level_sendto,addr,port)


/** Send command to set facility to facility to logger listening on addr:port address
 * @see send_uint32_by_function
 * @see set_facility
 */
#define set_facility_udp(facility,addr,port) \
	send_uint32_by_function(facility,set_facility_sendto,addr,port)

/** Send command to reset facility facility to logger listening on addr:port address
 * @see send_uint32_by_function
 * @see reset_facility
 */
#define reset_facility_udp(facility,addr,port) \
	send_uint32_by_function(facility,reset_facility_sendto,addr,port)

#endif	/* CONTROL_H */
