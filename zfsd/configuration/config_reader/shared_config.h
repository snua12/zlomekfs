#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <libconfig.h>
#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*add_mapping)(void * data, uint32_t id, string * name);

typedef void (*add_pair_mapping)(void * data, const char * local, const char * remote);

int read_node_list_shared_config(config_t * config);

int read_mapping_setting(config_setting_t * setting, add_mapping add, void * data);

int read_user_list_shared_config(config_t * config);

int read_group_list_shared_config(config_t * config);

int read_user_mapping_shared_config(config_t * config, const char * node_name, void * data);

int read_group_mapping_shared_config(config_t * config, const char * node_name, void * data);

int read_volume_list_shared_config(config_t *config, varray * volumes);

int read_volumes_layout_shared_config(config_t * config);

int read_shared_config(config_t * config);

#ifdef __cplusplus
}
#endif

#endif
