#include "system.h"
#include "config_common.h"
#include "log.h"
#include "zfs-prot.h"

uint16_t read_tcp_port_setting(config_setting_t * setting)
{
	config_setting_t * setting_node_port = config_setting_get_member(setting, "port");
	if (setting_node_port == NULL)
	{
		return ZFS_PORT;
	}

	if (config_setting_type(setting_node_port) != CONFIG_TYPE_INT)
	{
		message(LOG_WARNING, FACILITY_CONFIG, "TCP port has wrong type, it should be int, using default one.\n");
		return ZFS_PORT;
	}

	int tcp_port = config_setting_get_int(setting_node_port);
	// check port range
	if (tcp_port < 0 || tcp_port >= (1<<16))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "TCP  port is out of range, should be in 1..65536, using default one.\n");
		return ZFS_PORT;
	}

	return (uint16_t) tcp_port & 0xffff;
}
