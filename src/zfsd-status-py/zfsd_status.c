#include <dbus/dbus.h>
#include <stdio.h>

#include "zfsd_status.h"

zfsd_state_e ping_zfsd()
{
 DBusError err;
 DBusConnection * conn = NULL;
 int ret = 0;
 zfsd_state_e ret_code = ZFSD_STATE_UNKNOWN;
 DBusMessage* msg;
 DBusMessageIter args;
 DBusPendingCall* pending;
 int try = 0;


  // initialise the error value
  dbus_error_init (&err);

  // connect to the DBUS system bus, and check for errors
  conn = dbus_bus_get (DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set (&err)) { 
    fprintf (stderr, "Can't acquire bus: %s\n", err.message);
    dbus_error_free (&err); 
  }
  if (NULL == conn) { 
    goto FINISHING;
  }

  // create a new method call and check for errors
  msg = dbus_message_new_method_call (ZFSD_DBUS_NAME, // target for the method call
                                      "/zfsd/object", // object to call on - ignored
                                      ZFSD_DBUS_INTERFACE, // interface to call on
                                      ZFSD_STATUS_INFO_MESSAGE_NAME); // method name
  if (NULL == msg) { 
    fprintf (stderr, "Can't create new call\n");
    goto FINISHING;
  }

  // send message and get a handle for a reply
  if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
    fprintf (stderr, "Can't send message\n");
    goto FINISHING;
  }
  
  if (NULL == pending) { 
    fprintf (stderr, "Not pending\n");
    goto FINISHING;
  }
  dbus_connection_flush (conn);
  
  // free message
  dbus_message_unref (msg);
  
  // block until we recieve a reply
  dbus_pending_call_block (pending);

  // get the reply message
  for (try = 1; !dbus_pending_call_get_completed (pending) && try < 3; try++) {
    sleep (1);
  }

  if (!dbus_pending_call_get_completed (pending)) {
    dbus_pending_call_cancel (pending);
    fprintf (stderr, "Timeout in send\n");
    goto FINISHING;
  }

  msg = dbus_pending_call_steal_reply (pending);
  if (NULL == msg) {
    fprintf (stderr, "Can't get reply\n");
    goto FINISHING;
  }
  // free the pending message handle
  dbus_pending_call_unref (pending);

  // read the parameters
  if (!dbus_message_iter_init(msg, &args))
    fprintf (stderr, "no return code from ping\n");
  else if (ZFSD_STATUS_INFO_DBUS_TYPE != dbus_message_iter_get_arg_type(&args)) 
      fprintf(stderr, "Return code invalid!\n");
  else
    dbus_message_iter_get_basic(&args, &ret_code);

FINISHING:
  // free the message 
  if (msg)
    dbus_message_unref (msg);
  if (conn != NULL)
  {
    dbus_connection_unref(conn);
    conn = NULL;
  }

  return ret_code;
}

