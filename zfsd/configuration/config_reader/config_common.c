/**
 *  \file config_common.c
 * 
 *  \brief Implements common fuction for config reader
 *  \author Ales Snuparek 
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include "config_common.h"
#include "log.h"
#include "zfs-prot.h"

/*! \brief return tcp port form config setting
 *  \return port, in case of failure returns ZFS_PORT (default value)
 */
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

int config_setting_lookup_uint64_t(const config_setting_t *setting,
	const char *name, uint64_t *value)
{

#if (LIBCONFIG_VER_MAJOR >= 1) && (LIBCONFIG_VER_MINOR >= 4)
	int long_value;
#else
	long long_value;
#endif
	int rv = config_setting_lookup_int(setting, name, &long_value);
	if (rv != CONFIG_TRUE)
	{
		return rv;
	}

	*value = long_value;

	return CONFIG_TRUE;
}

