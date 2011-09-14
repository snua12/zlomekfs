#ifndef CONFIG_USER_H
#define CONFIG_USER_H

#include "system.h"
#include "fh.h"

/*! Read list of users from CONFIG_DIR/user_list.  */

bool
read_user_list (zfs_fh *config_dir);

#endif
