#ifndef CONFIG_COMMON_H
#define CONFIG_COMMON_H

#include <libconfig.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// reads port setting, if port setting is missing, or is invalid, then is returned ZFS_PORT
uint16_t read_tcp_port_setting(config_setting_t * setting);

#ifdef __cplusplus
}
#endif

#endif
