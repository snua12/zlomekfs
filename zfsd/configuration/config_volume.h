#ifndef CONFIG_VOLUME_H
#define CONFIG_VOLUME_H

#include "system.h"
#include "fh.h"

bool read_volume_list(zfs_fh * config_dir);

bool init_config_volume(void);

#endif
