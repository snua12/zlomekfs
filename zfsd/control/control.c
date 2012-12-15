
#include "system.h"
#include <string.h>
#include "dbus-init.h"
#include "log.h"
#include "control.h"

#ifdef ENABLE_CLI
#include "control_zfsd_cli.h"
#endif

bool initialize_control_c(void)
{
#ifdef ENABLE_DBUS
	start_dbus_control();
#endif

#ifdef ENABLE_CLI
	start_cli_control();
#endif
	return true;
}

void cleanup_control_c(void)
{
#ifdef ENABLE_DBUS
	stop_dbus_control();
#endif

#ifdef ENABLE_CLI
	stop_cli_control();
#endif
}
