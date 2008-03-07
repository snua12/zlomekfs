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

#include <unistd.h>
#include "dbus-provider.h"
#include "log.h"

#ifndef message
#define message(x)	/*no logging*/
#endif

#define DBUS_CONNECTION_TIMEOUT		1000

int dbus_provider_init (dbus_state_holder settings_struct)
{
#ifdef ENABLE_CHECKING
  if (settings_struct == NULL)
    return FALSE;
#endif
  memset (settings_struct, 0, sizeof (struct dbus_state_holder_def));
  dbus_threads_init_default ();

  if (pthread_mutex_init (&(settings_struct->mutex), NULL) != 0)
    message (LOG_ERROR, FACILITY_THREADING | FACILITY_DBUS,
             "can't init dbus mutex\n");

  return TRUE;
}

/** Dbus provider loop. waits for messages and call handlers on them.
    Just receive messages, doesn't do initialization or finalization
    To stop the loop, set the settings->connection to NULL.
 *
 * @param data pointer to initialized dbus_state_holder
 * @return NULL
 */
void * dbus_provider_loop (void * data)
{
  message_handle_state_e state = ZFSD_MESSAGE_HANDLED;
  dbus_state_holder settings = (dbus_state_holder) data;
  DBusMessage * msg = NULL;
  int listener_index = 0;
  
#ifdef ENABLE_CHECKING
  if (data == NULL)
  {
     message (LOG_ERROR, FACILITY_DBUS, "NULL settings struct in dbus_provider_loop\n");
    return NULL;
  }
#endif


  while (state == ZFSD_MESSAGE_HANDLED)
  {
    pthread_mutex_lock (&(settings->mutex));
    
    if (settings->connection == NULL)
    {
    	state = ZFSD_NO_MESSAGE;
    	goto NEXT;
    }
    
    // non blocking read of the next available message
    dbus_connection_read_write (settings->connection, DBUS_CONNECTION_TIMEOUT);
    msg = dbus_connection_pop_message (settings->connection);

    // loop again if we haven't got a message
    if (NULL == msg) {  
      goto NEXT;
    }

    message (LOG_DEBUG, FACILITY_DBUS, "received message '%s' on iface '%s'\n",
             dbus_message_get_member (msg), dbus_message_get_interface (msg));
    
    for (listener_index = 0; listener_index < settings->listener_count;
         listener_index ++)
    {
      message (LOG_LOOPS, FACILITY_DBUS, "trying listener %d\n", listener_index);
      state = settings->listeners[listener_index].handle_message 
                                                        (settings->connection,
                                                         &(settings-> error),
                                                         msg);
      if (state ==  ZFSD_MESSAGE_HANDLED)
      {
        break;
      }
    }
    if (state !=  ZFSD_MESSAGE_HANDLED)
    {
      message (LOG_WARNING, FACILITY_DBUS, "Can't handle message (%d)\n",
               state);
      state =  ZFSD_MESSAGE_HANDLED;
    }

    // free the message
    dbus_message_unref (msg);
    
NEXT:
    pthread_mutex_unlock (&(settings->mutex));
  }
  
  return NULL;
}

int dbus_provider_start (dbus_state_holder settings_struct, DBusBusType bus_type)
{
  int ret_code = TRUE;
  int listener_index = 0;
#ifdef ENABLE_CHECKING
  if (settings_struct == NULL || settings_struct->connection != NULL)
    return FALSE;
#endif

  pthread_mutex_lock (&(settings_struct->mutex));
  
  message (LOG_TRACE, FACILITY_DBUS, "Listening for method calls\n");

  // initialise the error
  dbus_error_init (&(settings_struct->error));
  
  // connect to the bus and check for errors
  settings_struct->connection = dbus_bus_get (bus_type, &(settings_struct->error));
  if (dbus_error_is_set (&(settings_struct->error))) { 
    message (LOG_ERROR, FACILITY_DBUS, "Connection Error (%s)\n", 
             settings_struct->error.message); 
    dbus_error_free (&(settings_struct->error)); 
  }
  if (NULL == settings_struct->connection) {
    message (LOG_ERROR, FACILITY_DBUS, "Connection Null\n");
    ret_code = FALSE;
    goto FINISHING;
  }
  

  for (listener_index = 0; listener_index < settings_struct->listener_count;
       listener_index ++ )
  {
    if (settings_struct->listeners [listener_index].add_name 
                                                    (settings_struct->connection,
                                                     &(settings_struct->error)
                                                    ) != TRUE)
      message (LOG_WARNING, FACILITY_DBUS, "Can't add name for listener");
    else
      message (LOG_DEBUG, FACILITY_DBUS, "Listener %d Added\n", listener_index);
  }
  
  if (pthread_create (&(settings_struct->loop_thread), NULL,
                  dbus_provider_loop, settings_struct) != 0)
    ret_code = FALSE;

FINISHING:
  if (ret_code != TRUE && settings_struct->connection != NULL)
  {
    dbus_provider_end (settings_struct);
  }
  
  pthread_mutex_unlock (&(settings_struct->mutex));

  return ret_code;
}

int dbus_provider_end (dbus_state_holder settings_struct)
{
  int listener_index = 0;
#ifdef ENABLE_CHECKING
  if (settings_struct == NULL || settings_struct->connection == NULL)
    return TRUE;
#endif

  pthread_mutex_lock (&(settings_struct->mutex));

  for (listener_index = 0; listener_index < settings_struct->listener_count; 
       listener_index ++)
  {
    if (settings_struct->listeners [listener_index].release_name 
                                                    (settings_struct->connection,
                                                     &(settings_struct->error)
                                                    ) != TRUE)
      message (LOG_INFO, FACILITY_DBUS, "Can't release name for listener");
  }
  
  dbus_connection_unref (settings_struct->connection);
  settings_struct->connection = NULL;
  dbus_error_free(&(settings_struct->error));

  pthread_mutex_unlock (&(settings_struct->mutex));
  
  sleep (DBUS_CONNECTION_TIMEOUT / 500);
  
  pthread_mutex_lock (&(settings_struct->mutex));
  
  pthread_cancel (settings_struct->loop_thread);
  pthread_join (settings_struct->loop_thread, NULL);
  settings_struct->loop_thread = 0;

  pthread_mutex_unlock (&(settings_struct->mutex));	
  
  return TRUE;
}

int dbus_provider_add_listener (dbus_state_holder settings_struct, 
                                dbus_name_add_t add_name,
                                dbus_name_release_t release_name,
                                dbus_message_handler_t handle_message)
{
  int ret_code = TRUE;
#ifdef ENABLE_CHECKING
  if (settings_struct == NULL || settings_struct->connection != NULL ||
      add_name == NULL || release_name == NULL || handle_message == NULL)
    return FALSE;
#endif
  pthread_mutex_lock (&(settings_struct->mutex));
  
  if (settings_struct->listener_count == MAX_DBUS_LISTENERS)
  {
    ret_code = FALSE;
    goto FINISHING;
  }
  
  dbus_listener current = &(settings_struct->listeners [settings_struct->listener_count ++]);

  current->add_name = add_name;
  current->release_name = release_name;
  current->handle_message = handle_message;

FINISHING:
  pthread_mutex_unlock (&(settings_struct->mutex));
  message (LOG_DEBUG, FACILITY_DBUS, "listener registration ended with %d\n", ret_code);
  return ret_code;
}

#include "zen-unit.h"

ZEN_TEST(add_listener){
  struct dbus_state_holder_def prov;
  void * add_name = 0x1;
  void * rel_name = 0x2;
  void * handle = 0x3;

  int ret =  dbus_provider_init (&prov);
  ZEN_ASSERT(ret == TRUE, "failed to initialize provider struct");

  ret = dbus_provider_add_listener (&prov, add_name, rel_name, handle);
  ZEN_ASSERT(ret == TRUE, "_add_listener has failed");
  ZEN_ASSERT(prov.listener_count == 1, "wrong listener count");
  ZEN_ASSERT(prov.listeners[0].handle_message == handle, "invalid handler set");
  ZEN_ASSERT(prov.listeners[0].add_name == add_name, "invalid add function set");
  ZEN_ASSERT(prov.listeners[0].release_name == rel_name, "invalid release function set");
  
  return PASS;
}
