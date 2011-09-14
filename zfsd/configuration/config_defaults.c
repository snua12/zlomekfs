#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "config_defaults.h"
#include "user-group.h"

/*! Set default node UID to UID of user NAME.  Return true on success.  */

bool
set_default_uid (const char *name)
{
  struct passwd *pwd;

  pwd = getpwnam (name);
  if (!pwd)
    return false;

  //TODO: ugly global variable
  default_node_uid = pwd->pw_uid;
  return true;
}

/*! Set default node GID to GID of group NAME.  Return true on success.  */

bool
set_default_gid (const char *name)
{
  struct group *grp;

  grp = getgrnam (name);
  if (!grp)
    return false;

  default_node_gid = grp->gr_gid;
  return true;
}

/*! Set default local user/group.  */

void
set_default_uid_gid (void)
{
  set_default_uid ("nobody");
  if (!set_default_gid ("nogroup"))
    set_default_gid ("nobody");
}

