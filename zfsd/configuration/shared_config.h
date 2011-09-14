#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <libconfig.h>
#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ! \brief Structure for storing informations about volumes from shared config. */
typedef struct volume_entry_def
{
	uint32_t id; /* !< ID of volume */
	string name; /* !< name of volume */
	string mountpoint; /* !< mountpoint of volume */
	string master_name; /* !< name of master node */
	varray slave_names; /* ! names of slave nodes */
}
volume_entry;



typedef void (*add_mapping)(uint32_t id, string * name);

int read_node_list_shared_config(config_t * config);

int read_mapping_setting(config_setting_t * setting, add_mapping add);

int read_user_list_shared_config(config_t * config);

int read_group_list_shared_config(config_t * config);

int read_user_mapping_shared_config(config_t * config);

int read_group_mapping_shared_config(config_t * config);

int read_volume_list_shared_config(config_t *config, varray * volumes);

int read_volumes_layout_shared_config(config_t * config);

int read_shared_config(config_t * config);

#ifdef __cplusplus
}
#endif

#endif
