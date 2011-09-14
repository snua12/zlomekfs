#ifndef DBUS_PROVIDER_H
#define DBUS_PROVIDER_H

/*! \file
    \brief Dbus universal listener api.  */

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
   or download it from http://www.gnu.org/licenses/gpl.html */


#include <dbus/dbus.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
 
/// how much components should listen simultaneously
#define MAX_DBUS_LISTENERS	2

/// Enum of message handling result
typedef enum {
  /// message was successfully handled
  ZFSD_MESSAGE_HANDLED = 0,
  /// message type is not known
  ZFSD_MESSAGE_UNKNOWN = 1,
  /// no message provided
  ZFSD_NO_MESSAGE = 2,
  /// error occured during handling
  ZFSD_HANDLE_ERROR = 3
} message_handle_state_e;

/** Try to handle dbus message.
    Component should check if know message and try to handle it
    if component doesn't know message it should return ZFSD_MESSAGE_UNKNONW
 *
 * @param conn valid dbus connection with syplog names registered
 * @param err_struct initialized dbus error struct
 * @param msg message received
 * @return ZFSD_MESSAGE_HANDLED if handled,
           ZFSD_HANDLE_ERROR if message is known bud error occured in processing,
           ZFSD_MESSAGE_UKNONWN if zfsd doesn't known message type
 */
typedef message_handle_state_e (*dbus_message_handler_t) (DBusConnection * conn, 
                                       DBusError * err_struct,
                                       DBusMessage * msg);

/** Release names from dbus connection.
    Component should release all dbus names
    and signal match rules which is adding in dbus_name_add_t.
 *
 * @param connection initialized dbus connection
 * @param err_struct initialized dbus error struct 
 * @return TRUE if successfully released, FALSE otherwise
*/
typedef int (*dbus_name_release_t) (DBusConnection * connection,
                                    DBusError * err_struct);

/** Register names to dbus connection
    Component should add dbus names in which wants to listen
    and signal match rules.
 *
 * @param connection initialized dbus connection
 * @param err_struct initialized dbus error struct 
 * @return TRUE if successfully added, FALSE otherwise
*/
typedef int (*dbus_name_add_t) (DBusConnection * connection, 
                                DBusError * err_struct);

/// structure holding info about listening component
typedef struct dbus_listener_def
{
  /** function called to register dbus names of component
   * @see dbus_name_add_t
  */
  dbus_name_add_t add_name;

  /** function called to unregister dbus names of component
   * @see dbus_name_release_t
  */
  dbus_name_release_t release_name;

  /** function called to handle message
   * @see dbus_message_handler_t
  */
  dbus_message_handler_t handle_message;
} * dbus_listener;

/// structure holding dbus_provider state (loop and components info)
typedef struct dbus_state_holder_def
{
  /** Connection for receiving and senging messages.
      If NULL, the listening loop will terminate
  */
  DBusConnection * connection;
  /// dbus error srtucture
  DBusError error;

  /// count of registered listeners (and valid entries in listeners struct too)
  int listener_count;
  /// specification of registered listener components
  struct dbus_listener_def listeners [MAX_DBUS_LISTENERS];

  /// thread id of provider listening loop
  pthread_t loop_thread;
  /// mutex to lock this structure
  pthread_mutex_t mutex;
} * dbus_state_holder;

/** Initialize dbus provider struct
 *
 * @param settings_struct valid pointer to uninitialized settings struct
 * @return TRUE if sucessfully initialized (FALSE otherwise)
*/
int dbus_provider_init (dbus_state_holder settings_struct);

/** Add listener (component wanting to listen for messages) to provider
    The provider loop should not run upon this call
 *
 * @param settins_struct initialized struct of provider which doesn't run
 * @param add_name function to call to add component names to connection
 * @param release_name function to call to remove component names to connection
 * @param handle_message function to call on received message (try) to handle it
 * @return TRUE if added, FALSE otherwise
 * @see dbus_name_add_t
 * @see dbus_name_release_t
 * @see dbus_message_handler_t
*/
int dbus_provider_add_listener (dbus_state_holder settings_struct, 
                                dbus_name_add_t add_name,
                                dbus_name_release_t release_name,
                                dbus_message_handler_t handle_message);



/** Open dbus connection and dispatch listening thread
 *
 * @param settins_struct initialized provider struct with added listeners (not running)
 * @param bus_type on which bus to listen (system, session or what)
 * @param TRUE if thread started, FALSE otherwise
*/
int dbus_provider_start (dbus_state_holder settings_struct, DBusBusType bus_type);

/** Stop listening thread and close dbus connection
 *
 * @param settings_struct valid pointer to running provider struct
 * @return TRUE if terminated, FALSE otherwise
*/
int dbus_provider_end (dbus_state_holder settings_struct);

#ifdef __cplusplus
}
#endif

#endif	/* DBUS_PROVIDER_H */
