/* ! \file \brief Implementation of control client functions (high level
   logger-control interface) \see control.h */

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include <unistd.h>
#include <stdio.h>

#include "control.h"
#include "errno.h"

/** resolve address to struct in_addr
 * @param addr IP address only so far
 * @param target valid pointer to struct in_addr where to store resolved address
 * @return ERR_BAD_PARAMS, NOERR
*/
static syp_error resolve_host(const char *addr, struct in_addr *target)
{
	/* TODO: use int getaddrinfo(const char *node, const char *service, const
	   struct addrinfo *hints, struct addrinfo **res); */
	if (inet_aton(addr, target) == 0)
		return ERR_BAD_PARAMS;

	return NOERR;

}


syp_error send_uint32_by_function(uint32_t data,
								  syp_error(*function) (int, uint32_t,
														const struct sockaddr
														*, socklen_t),
								  const char *ip, uint16_t port)
{
	struct sockaddr_in addr;
	int sock = -1;
	syp_error ret_code = NOERR;

	if (ip == NULL)
		ip = DEFAULT_COMMUNICATION_ADDRESS;
	if (port == 0)
		port = DEFAULT_COMMUNICATION_PORT;

	// init socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		ret_code = sys_to_syp_error(errno);
		goto FINISHING;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	ret_code = resolve_host(ip, &(addr.sin_addr));
	if (ret_code != NOERR)
		goto FINISHING;

	ret_code =
		function(sock, data, (struct sockaddr *)&addr,
				 sizeof(struct sockaddr_in));

  FINISHING:
	if (sock >= 0)
		close(sock);

	return ret_code;
}

syp_error set_level_udp(log_level_t level, const char *addr, uint16_t port)
{
	return send_uint32_by_function(level, set_level_sendto, addr, port);
}

syp_error set_facility_udp(facility_t facility, const char *addr,
						   uint16_t port)
{
	return send_uint32_by_function(facility, set_facility_sendto, addr, port);
}

syp_error reset_facility_udp(facility_t facility, const char *addr,
							 uint16_t port)
{
	return send_uint32_by_function(facility, reset_facility_sendto, addr,
								   port);
}

static syp_error dbus_connect(DBusConnection ** connection)
{
	DBusError err;
	int ret = 0;
	syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
	if (connection == NULL)
		return ERR_BAD_PARAMS;
#endif

	// initialise the error value
	dbus_error_init(&err);

	// connect to the DBUS system bus, and check for errors
	*connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err))
	{
		fprintf(stderr, "Can't acquire bus: %s\n", err.message);
		dbus_error_free(&err);
	}
	if (NULL == *connection)
	{
		ret_code = ERR_DBUS;
		goto FINISHING;
	}

	// register our name on the bus, and check for errors
	ret =
		dbus_bus_request_name(*connection, SYPLOG_DEFAULT_DBUS_SOURCE,
							  DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (dbus_error_is_set(&err))
	{
		fprintf(stderr, "Can't get name: %s\n", err.message);
		dbus_error_free(&err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret)
	{
		fprintf(stderr, "Not primary owner\n");
		ret_code = ERR_DBUS;
		goto FINISHING;
	}


  FINISHING:
	if (ret_code != NOERR && (*connection) != NULL)
	{
		dbus_bus_release_name(*connection, SYPLOG_DEFAULT_DBUS_SOURCE, NULL);
		dbus_connection_unref(*connection);
		*connection = NULL;
	}

	return ret_code;
}

static syp_error dbus_disconnect(DBusConnection ** connection)
{

#ifdef ENABLE_CHECKING
	if (connection == NULL)
		return ERR_BAD_PARAMS;
	if (*connection == NULL)
		return ERR_NOT_INITIALIZED;
#endif

	dbus_bus_release_name(*connection, SYPLOG_DEFAULT_DBUS_SOURCE, NULL);
	dbus_connection_unref(*connection);
	*connection = NULL;

	return NOERR;
}

// -------------------------- DBUS -----------------------------------

/**
 * Connect to the DBUS bus and send a broadcast signal
 * TODO: non broadcasting signals
 */
static syp_error dbus_sendsignal(const char *target UNUSED,
								 char *signal_name, int value_type,
								 void *signal_value)
{
	DBusMessage *msg = NULL;
	DBusMessageIter args;
	DBusConnection *conn;
	syp_error ret_code = NOERR;
	ret_code = dbus_connect(&conn);
	if (ret_code != NOERR || conn == NULL)
		goto FINISHING;

	// create a signal & check for errors 
	msg = dbus_message_new_signal(SYPLOG_DEFAULT_DBUS_OBJECT,	// object name 
																// 
								  // of the
								  // signal
								  SYPLOG_DBUS_INTERFACE,	// interface name
								  // of the signal
								  signal_name);	// name of the signal
	if (NULL == msg)
	{
		ret_code = ERR_DBUS;
		goto FINISHING;
	}

	// append arguments onto signal
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, value_type, signal_value))
	{
		fprintf(stderr, "No memory in init append\n");
		ret_code = ERR_DBUS;
		goto FINISHING;
	}

	// send the message and flush the connection
	if (!dbus_connection_send(conn, msg, NULL))
	{
		fprintf(stderr, "Can't send message\n");
		ret_code = ERR_DBUS;
		goto FINISHING;
	}
	dbus_connection_flush(conn);

  FINISHING:
	// free the message 
	if (msg)
		dbus_message_unref(msg);
	if (conn)
		dbus_disconnect(&conn);

	return ret_code;
}

/**
 * Call a method on a remote object
 */
static const void *dbus_query(DBusConnection * conn, const char *target_name,
							  char *method_name, int arg_type,
							  void *method_arg)
{
	DBusMessage *msg;
	DBusMessageIter args;
	DBusPendingCall *pending;
	int try = 0;
	const char *response = NULL;

	if (target_name == NULL)
		target_name = SYPLOG_DEFAULT_DBUS_TARGET;

	// create a new method call and check for errors
	msg = dbus_message_new_method_call(target_name,	// target for the method
									   // call
									   SYPLOG_DEFAULT_DBUS_OBJECT,	// object
									   // to call 
									   // on
									   SYPLOG_DBUS_INTERFACE,	// interface
									   // to call on
									   method_name);	// method name
	if (NULL == msg)
	{
		fprintf(stderr, "Can't create new call\n");
		response = NULL;
		goto FINISHING;
	}

	// append arguments
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, arg_type, &method_arg))
	{
		fprintf(stderr, "No memory in init_append\n");
		response = NULL;
		goto FINISHING;
	}

	// send message and get a handle for a reply
	if (!dbus_connection_send_with_reply(conn, msg, &pending, -1))
	{							// -1 is default timeout
		fprintf(stderr, "Can't send message\n");
		response = NULL;
		goto FINISHING;
	}

	if (NULL == pending)
	{
		fprintf(stderr, "Not pending\n");
		response = NULL;
		goto FINISHING;
	}
	dbus_connection_flush(conn);


	// free message
	dbus_message_unref(msg);

	// block until we recieve a reply
	dbus_pending_call_block(pending);

	// get the reply message
	for (try = 1; !dbus_pending_call_get_completed(pending) && try < 3; try++)
	{
		sleep(1);
	}

	if (!dbus_pending_call_get_completed(pending))
	{
		dbus_pending_call_cancel(pending);
		fprintf(stderr, "Timeout in send\n");
		response = NULL;
		goto FINISHING;
	}

	msg = dbus_pending_call_steal_reply(pending);
	if (NULL == msg)
	{
		fprintf(stderr, "Can't get reply\n");
		response = NULL;
		goto FINISHING;
	}
	// free the pending message handle
	dbus_pending_call_unref(pending);

	// read the parameters
	if (!dbus_message_iter_init(msg, &args))
		fprintf(stderr, "no reply code to ping\n");
	else if (SYPLOG_PING_DBUS_TYPE != dbus_message_iter_get_arg_type(&args))
		fprintf(stderr, "Wrong reply type to ping\n");
	else
		dbus_message_iter_get_basic(&args, &response);


  FINISHING:
	// free the message 
	if (msg)
		dbus_message_unref(msg);

	return response;
}

#define PING_STR		"ping"

syp_error ping_syplog_dbus(const char *logger_name)
{
	DBusConnection *conn = NULL;
	syp_error ret_code = NOERR;

	ret_code = dbus_connect(&conn);
	if (ret_code != NOERR || conn == NULL)
		goto FINISHING;

	const char *data = dbus_query(conn, logger_name, SYPLOG_MESSAGE_PING_NAME,
								  SYPLOG_PING_DBUS_TYPE, PING_STR);
	if (data == NULL)
	{
		ret_code = ERR_DBUS;
		goto FINISHING;
	}
	if (strncmp(data, PING_STR, 4) != 0)
	{
		ret_code = ERR_DBUS;
	}

  FINISHING:
	if (conn)
		dbus_disconnect(&conn);
	return ret_code;
}

syp_error set_level_dbus(log_level_t level, const char *logger_name)
{
	return dbus_sendsignal(logger_name, SYPLOG_SIGNAL_SET_LOG_LEVEL_NAME,
						   SYPLOG_LOG_LEVEL_DBUS_TYPE, &level);
}

syp_error set_facility_dbus(facility_t facility, const char *logger_name)
{
	return dbus_sendsignal(logger_name, SYPLOG_SIGNAL_SET_FACILITY_NAME,
						   SYPLOG_FACILITY_DBUS_TYPE, &facility);
}

syp_error reset_facility_dbus(facility_t facility, const char *logger_name)
{
	return dbus_sendsignal(logger_name, SYPLOG_SIGNAL_RESET_FACILITY_NAME,
						   SYPLOG_FACILITY_DBUS_TYPE, &facility);
}
