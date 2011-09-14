#include "system.h"
#ifndef CONFIG_DEAFULT_UID_GID
#define CONFIG_DEAFULT_UID_GID
void set_default_uid_gid(void);

bool set_default_uid(const char *name);

bool set_default_gid(const char *name);

#endif
