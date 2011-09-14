#ifndef CONFIG_USER_MAPPING_H
#define CONFIG_USER_MAPPING_H

#include "system.h"
#include "fh.h"

bool read_user_mapping(zfs_fh * user_dir, uint32_t sid);

#endif
