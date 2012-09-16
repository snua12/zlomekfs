#ifndef CONFIG_USER_GROUP_H
#define CONFIG_USER_GROUP_H

#include "system.h"
#include "fh.h"

#ifdef __cplusplus
extern "C" 
{
#endif

/*! Read list of users from CONFIG_DIR/user_list.  */
extern bool read_user_list(zfs_fh * config_dir);

/*! Read list of groups from CONFIG_DIR/group_list.  */
extern bool read_group_list(zfs_fh * config_dir);

/*! Read list of user mapping.  If SID == 0 read the default user mapping
   from CONFIG_DIR/user/default else read the special mapping for node SID.  */
extern bool read_user_mapping(zfs_fh * user_dir, uint32_t sid);


/*! Read list of group mapping.  If SID == 0 read the default group mapping
   from CONFIG_DIR/group/default else read the special mapping for node SID.  */
bool read_group_mapping(zfs_fh * group_dir, uint32_t sid);

#ifdef __cplusplus
}
#endif

#endif
