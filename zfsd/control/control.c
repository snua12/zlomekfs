
#include "system.h"
#include <string.h>
#include "dbus-init.h"
#include "log.h"
#include "control.h"

bool initialize_control_c(void)
{
#ifdef ENABLE_DBUS
  start_dbus_control();
#endif
  return true;
}

void cleanup_control_c(void)
{
#ifdef ENABLE_DBUS
  stop_dbus_control();
#endif
}

