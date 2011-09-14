#ifndef CONFIG_VOLUME_H
#define CONFIG_VOLUME_H

#include "system.h"
#include "fh.h"

void
read_volume_hierarchy(zfs_fh * volume_hierarchy_dir, uint32_t vid,
					  string * name, string * mountpoint);

bool read_volume_list(zfs_fh * config_dir);


bool init_config_volume(void);
#endif
