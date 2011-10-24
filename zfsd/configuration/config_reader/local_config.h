#ifndef LOCAL_CONFIG_H
#define LOCAL_CONFIG_H

#include <libconfig.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// reads volumes config from local config
int read_volumes_local_config(config_t * config, bool reread);

/// reads volumes config from local config file
int read_volumes_local_config_from_file(const char * local_path, bool reread);

/// reads users local cofnig
int read_users_local_config(config_t * config);

/// reads group local config
int read_groups_local_config(config_t * config);

/// reads information about this node from local config
int read_this_node_local_config(config_t * config);

/// reads information about config node from local config
int read_config_node_local_config(config_t * config);

/// reads system specific config from local config
int read_system_specific_config(config_t * config);

/// reads thread limits from local config
int read_threads_config(config_t * config);

#ifdef ENABLE_VERSIONS
/// reads versioning config
int read_versioning_config(config_t * config);
#endif

/// reads local configuration
int read_local_config(config_t * config);

/// reads local config from selected file
int read_local_config_from_file(const char * local_path);

#ifdef __cplusplus
}
#endif

#endif

