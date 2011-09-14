#include "system.h"
#include "dbus-provider.h"
#include "dbus-zfsd-service.h"
#include "log.h"
#include "syplog.h"
#include "syplog/listener.h"
#include "dbus-init.h"

struct listener_def control;

static int dbus_add_log_name (DBusConnection * connection, 
                       DBusError * err_struct)
{
  syp_error ret = dbus_add_syplog_name (connection, err_struct, &syplogger);
  if (ret == NOERR)
    return TRUE;
  else
    return FALSE;
}

static int dbus_release_log_name (DBusConnection * connection, 
                           DBusError * err_struct)
{
  syp_error ret = dbus_release_syplog_name (connection, err_struct, &syplogger);
  if (ret == NOERR)
    return TRUE;
  else
    return FALSE;
}

static message_handle_state_e dbus_handle_log_message (DBusConnection * connection, 
                                                DBusError * err_struct,
                                                DBusMessage * msg)
{
  syp_error ret = dbus_handle_syplog_message (connection, err_struct, msg, &syplogger);
  switch (ret)
  {
    case NOERR:
      return ZFSD_MESSAGE_HANDLED;
      break;
    case ERR_BAD_MESSAGE:
      return ZFSD_MESSAGE_UNKNOWN;
      break;
    default:
      return ZFSD_HANDLE_ERROR;
  }

  return ZFSD_HANDLE_ERROR;

}

static struct dbus_state_holder_def dbus_provider;

void stop_dbus_control(void)
{
  if (dbus_provider_init (&dbus_provider) != TRUE)
    message (LOG_WARNING, FACILITY_DBUS | FACILITY_ZFSD,
             "Can't initialize dbus provider\n");
  else
  {

    if (dbus_provider_add_listener (&dbus_provider, dbus_add_zfsd_name, 
                                dbus_release_zfsd_name, dbus_handle_zfsd_message)
        != TRUE)
      message (LOG_WARNING, FACILITY_DBUS | FACILITY_ZFSD,
               "Can't add dbus zfsd state provider\n");


    if (dbus_provider_add_listener (&dbus_provider, dbus_add_log_name, 
                                dbus_release_log_name, dbus_handle_log_message)
        != TRUE)
      message (LOG_WARNING, FACILITY_DBUS | FACILITY_ZFSD,
               "Can't add dbus log control\n");

    if (dbus_provider_start (&dbus_provider, DBUS_BUS_SYSTEM) != TRUE)
      message (LOG_ERROR, FACILITY_DBUS | FACILITY_ZFSD,
               "Can't start dbus provider\n");
  }

}

void start_dbus_control(void)
{
  dbus_provider_end (&dbus_provider);
}
