#ifndef READ_CONFIG_H
#define READ_CONFIG_H

#include "system.h"
#include "fh.h"
#include "memory.h"

bool
read_config_file (const char *file);

bool
read_node_list (zfs_fh *config_dir);

bool 
read_local_volume_info (string *path, bool reread);

extern string local_config;

#endif
