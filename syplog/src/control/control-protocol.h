#ifndef		CONTROL_PROTOCOL_H
#define		CONTROL_PROTOCOL_H
/*! \file
    \brief Network protocol for controlling logger remotely. (deffinitions)

  Defines wrapper functions for sending and receiving messages,
  defines network specific operations and transformations,
  doesn't define what to do with received data.

  This functions shoud be used in some listen loop,
  data received may be checked to be valid and
  if logger needs to be locked, caller should ensure this.

  On server side (logger listen) *_receive should be called.
  Caller should check the type of first message on network stack
  beforehand (for example by PEEK the type).
  On client side (remote control) *_send functions should be called.
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


#define _GNU_SOURCE
#include <arpa/inet.h>
#undef _GNU_SOURCE

#include "log-level.h"
#include "facility.h"

/// default port to listen on when using udp
#define DEFAULT_COMMUNICATION_PORT	12345

/// default ip address on which to listen when using udp control
#define DEFAULT_COMMUNICATION_ADDRESS	"127.0.0.1"

#include <dbus/dbus.h>


/// timeout for receiving message in miliseconds
#define	DBUS_WAIT_TIMEOUT			1000
/// default source name
#define	SYPLOG_DEFAULT_DBUS_SOURCE		"syplog.default.source"
/// default target name
#define SYPLOG_DEFAULT_DBUS_TARGET		"syplog.default.target"
/// default object to call on
#define	SYPLOG_DEFAULT_DBUS_OBJECT		"/syplog/default/control"
/// interface name for control syplog
#define	SYPLOG_DBUS_INTERFACE			"syplog.signal.control"
/// name of the signal for setting facility
#define	SYPLOG_SIGNAL_SET_FACILITY_NAME		"set_facility"
/// name of the signal for reseting facility
#define	SYPLOG_SIGNAL_RESET_FACILITY_NAME	"reset_facility"
/// name of the signal for setting log leve
#define	SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME	"set_log_level"
/// name of the ping message
#define SYPLOG_MESSAGE_PING_NAME		"ping"

/// type of payload data for log level
#define SYPLOG_LOG_LEVEL_DBUS_TYPE		DBUS_TYPE_UINT32
/// type of payload data for facility
#define SYPLOG_FACILITY_DBUS_TYPE		DBUS_TYPE_UINT32
/// type of payload data for ping
#define SYPLOG_PING_DBUS_TYPE			DBUS_TYPE_STRING


#define	SYPLOG_SIGNAL_RECEIVE_RULE		"type='signal',interface='" SYPLOG_DBUS_INTERFACE "'"

/** communication types enum,
 * used as internal discriminator in listener struct
 */
typedef enum communication_type_def
{
  /// uninitialized listener
  COMM_NONE = 0,
  /// listener listens on udp
  COMM_UDP = 1,
  /// listener listens on dbus
  COMM_DBUS = 2
} communication_type;


/** types of messages
 * this is the first thing that must go in every message
 * the rest is type dependent
 */
typedef enum message_type_def
{
  /// ping the logger (check listening)
  MESSAGE_PING = 0,
  /** set log level of the logger
   * @see set_log_level
  */
  MESSAGE_SET_LEVEL = 2,
  /** set facility as being logged
   * @see set_facility
   */
  MESSAGE_SET_FACILITY = 4,
  /** unset facility - do not log messages
   * from this facility
   * @see reset_facility
   */
  MESSAGE_RESET_FACILITY = 8
} message_type;


/** define sending function of set_level message for statefull protocols
 * @see set_level_sendto
 */
#define set_level_send(socket, level)	set_level_sendto(socket,level,NULL,0)

/** marshall and send message for setting log level
 * sends message in format <message_type><log_level_t> in net format (BE)
 * message_type will be MESSAGE_SET_LEVEL
 * length of message send is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param level log level to set on remote logger
 * @param to address of receiver in case of connection-less protocols, NULL otherwise
 * @param tolen length (sizeof) to address
 * @return std errors
 * @see set_log_level
 */
syp_error set_level_sendto (int socket, log_level_t level, 
  const struct sockaddr * to, socklen_t tolen);


/** define receiving function of set_level message for statefull protocols
 * @see set_level_receive_from
 */
#define set_level_receive(socket,level)	set_level_receive_from(socket,level,NULL,NULL)

/** receive and unmarshal message for setting log level
 * remove data from queue
 * expects message in format <message_type><log_level_t> in net format (endianity)
 * message_type should be MESSAGE_SET_LEVEL
 * length of message read is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param level valid pointer to log_level_t. log level to be set will be filled
 * @param from valid pointer where to put sender address or NULL \
 address of sender in case of connection-less protocols will be filled, NULL otherwise
 * @param fromlen valid pointer or NULL. if valid, will indicate length of from address
 * @return std errors
 * @see set_log_level
 */
syp_error set_level_receive_from (int socket, log_level_t * level,
  struct sockaddr * from, socklen_t * fromlen);

/** define sending function of set_facility message for statefull protocols
 * @see set_facility_sendto
 */
#define set_facility_send(socket,facility)	\
	set_facility_sendto(socket,facility,NULL,0)

/** marshall and send message for setting facility as to be logged
 * sends message in format <message_type><facility_t> in net format (BE)
 * message_type will be MESSAGE_SET_FACILITY
 * length of message send is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param facility facility to enable for logging on remote logger
 * @param to address of receiver in case of connection-less protocols, NULL otherwise
 * @param tolen length (sizeof) to address
 * @return std errors
 * @see set_log_level
 */
syp_error set_facility_sendto (int socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen);

/** define receiving function of set_facility message for statefull protocols
 * @see set_facility_receive_from
 */
#define set_facility_receive(socket,facility)	\
	set_facility_receive_from(socket,facility,NULL,NULL)

/** receive and unmarshal message for setting facility as to be logged
 * remove data from queue
 * expects message in format <message_type><facility_t> in net format (BE)
 * message_type should be MESSAGE_SET_FACILITY
 * length of message read is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param facility valid pointer where to put the facility (facilities) received
 * @param from valid pointer where to put sender address or NULL \
 address of sender in case of connection-less protocols will be filled, NULL otherwise
 * @param fromlen valid pointer or NULL. if valid, will indicate length of from address
 * @return std errors
 * @see reset_facility
 */
syp_error set_facility_receive_from (int socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen);

/** define sending function of reset_facility message for statefull protocols
 * @see reset_facility_sendto
 */
#define reset_facility_send(socket,facility)	\
	reset_facility_sendto(socket,facility,NULL,0)

/** marshall and send message for resetting facility (set as not to be logged)
 * sends message in format <message_type><facility_t> in net format (BE)
 * message_type will be MESSAGE_RESET_FACILITY
 * length of message send is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param facility facility to enable for logging on remote logger
 * @param to address of receiver in case of connection-less protocols, NULL otherwise
 * @param tolen length (sizeof) to address
 * @return std errors
 * @see reset_facility
 */
syp_error reset_facility_sendto (int socket, facility_t facility, 
  const struct sockaddr * to, socklen_t tolen);

/** define receiving function of reset_facility message for statefull protocols
 * @see reset_facility_receive_from
 */
#define reset_facility_receive(socket,facility)	\
	reset_facility_receive_from(socket,facility,NULL,NULL)

/** receive and unmarshal message for removing facility ( set as not to be logged)
 * remove data from queue
 * expects message in format <message_type><facility_t> in net format (BE)
 * message_type should be MESSAGE_RESET_FACILITY
 * length of message read is sizeof(uint32_t) * 2
 *
 * @param socket initialized socket. if statefull protocol is used, socket must be connected
 * @param facility valid pointer where to put the facility (facilities) received
 * @param from valid pointer where to put sender address or NULL \
 address of sender in case of connection-less protocols will be filled, NULL otherwise
 * @param fromlen valid pointer or NULL. if valid, will indicate length of from address
 * @return std errors
 * @see reset_facility
 */
syp_error reset_facility_receive_from (int socket, facility_t * facility,
  struct sockaddr * from, socklen_t * fromlen);

#endif	/* CONTROL_PROTOCOL_H */

