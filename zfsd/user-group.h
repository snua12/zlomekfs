/* User and group functions.
   Copyright (C) 2003 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef USER_GROUP_H
#define USER_GROUP_H

#include "system.h"
#include <inttypes.h>
#include "hashtab.h"

/* Description of ZFS user.  */
typedef struct user_def
{
  uint32_t id;			/* user ID */
  uint32_t gid;			/* group ID of default group */
  char *name;			/* name of the user */
  htab_t groups;		/* list of groups user is in */
} *user_t;

/* Description of ZFS group.  */
typedef struct group_def
{
  uint32_t id;			/* group ID */
  char *name;			/* name of the group */
} *group_t;

extern user_t user_create (uint32_t id, char *name, uint32_t gid);
extern void set_default_groups ();
extern void user_destroy (user_t u);
extern group_t group_create (uint32_t id, char *name, char *user_list);
extern void group_destroy (group_t g);
extern void initialize_user_group_c ();
extern void cleanup_user_group_c ();

#endif
