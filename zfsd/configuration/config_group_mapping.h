#ifndef CONFIG_GROUP_MAPPING_H
#define CONFIG_GROUP_MAPPING_H

/* ! Read list of group mapping.  If SID == 0 read the default group mapping
   from CONFIG_DIR/group/default else read the special mapping for node SID.  */

bool read_group_mapping(zfs_fh * group_dir, uint32_t sid);

#endif
