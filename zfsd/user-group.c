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

#include "system.h"
#include <string.h>
#include "pthread.h"
#include "log.h"
#include "memory.h"
#include "user-group.h"
#include "hashtab.h"
#include "alloc-pool.h"
#include "crc32.h"

/* Tables of users and groups, searched by ID and by NAME.  */
static htab_t users_id;
static htab_t users_name;
static htab_t groups_id;
static htab_t groups_name;

/* Mutex protecting hash tables users_* and groups_*.  */
pthread_mutex_t users_groups_mutex;

/* Hash functions for user and group ID.  */
#define USER_ID_HASH(ID) (ID)
#define GROUP_ID_HASH(ID) (ID)

/* Hash functions for user and group NAME.  */
#define USER_NAME_HASH(NAME) (crc32_string (NAME))
#define GROUP_NAME_HASH(NAME) (crc32_string (NAME))

/* Hash function for user X, computed from ID.  */

static hash_t
users_id_hash (const void *x)
{
  return USER_ID_HASH (((user) x)->id);
}

/* Hash function for user X, computed from NAME.  */

static hash_t
users_name_hash (const void *x)
{
  return USER_NAME_HASH (((user) x)->name);
}

/* Compare an user XX with user ID YY.  */

static int
users_id_eq (const void *xx, const void *yy)
{
  return ((user) xx)->id == *(unsigned int *) yy;
}

/* Compare an user XX with user name YY.  */

static int
users_name_eq (const void *xx, const void *yy)
{
  return strcmp (((user) xx)->name, (const char *) yy);
}

/* Hash function for group X, computed from ID.  */

static hash_t
groups_id_hash (const void *x)
{
  return USER_ID_HASH (((group) x)->id);
}

/* Hash function for group X, computed from NAME.  */

static hash_t
groups_name_hash (const void *x)
{
  return USER_NAME_HASH (((group) x)->name);
}

/* Compare a group XX with group ID YY.  */

static int
groups_id_eq (const void *xx, const void *yy)
{
  return ((group) xx)->id == *(unsigned int *) yy;
}

/* Compare a group XX with group name YY.  */

static int
groups_name_eq (const void *xx, const void *yy)
{
  return strcmp (((group) xx)->name, (const char *) yy);
}

/* Create an user with ID and NAME with default group GID.  */

user
user_create (unsigned int id, char *name, unsigned int gid)
{
  user u;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (users_id, &id, USER_ID_HASH (id),
				    NO_INSERT);
  if (slot1)
    {
      /* ID is already there.  */
      return NULL;
    }
  slot2 = htab_find_slot_with_hash (users_name, name, USER_NAME_HASH (name),
				    NO_INSERT);
  if (slot2)
    {
      /* NAME is already there.  */
      return NULL;
    }

  slot1 = htab_find_slot_with_hash (users_id, &id, USER_ID_HASH (id), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1)
    abort ();
#endif
  slot2 = htab_find_slot_with_hash (users_name, name, USER_NAME_HASH (name),
				    INSERT);
#ifdef ENABLE_CHECKING
  if (!slot2)
    abort ();
#endif

  u = (user) xmalloc (sizeof (*u));
  u->id = id;
  u->gid = gid;
  u->name = xstrdup (name);
  u->groups = htab_create (5, groups_id_hash, groups_id_eq, NULL, NULL);
  *slot1 = u;
  *slot2 = u;

  return u;
}

/* For each user, add default group to user's list of groups.  */

void
set_default_groups ()
{
  void **slot;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  HTAB_FOR_EACH_SLOT (users_id, slot,
    {
      user u = (user) slot;
      group g;

      g = (group) htab_find_with_hash (groups_id, &u->gid,
				       GROUP_ID_HASH (u->gid));
      if (!g)
	continue;

      slot2 = htab_find_slot_with_hash (u->groups, &u->gid,
					GROUP_ID_HASH (u->gid), INSERT);
      if (!*slot2)
	*slot2 = g;
    });
}

/* Destroy user U.  */

void
user_destroy (user u)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot = htab_find_slot_with_hash (users_id, &u->id, USER_ID_HASH (u->id),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (users_id, slot);

  slot = htab_find_slot_with_hash (users_name, u->name,
				   USER_NAME_HASH (u->name), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (users_name, slot);

  htab_destroy (u->groups);
  free (u->name);
  free (u);
}

/* Create a group with ID and NAME, its list of users is USER_LIST.  */

group
group_create (unsigned int id, char *name, char *user_list)
{
  group g;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (groups_id, &id, GROUP_ID_HASH (id),
				    NO_INSERT);
  if (slot1)
    {
      /* ID is already there.  */
      return NULL;
    }
  slot2 = htab_find_slot_with_hash (groups_name, name, GROUP_NAME_HASH (name),
				    NO_INSERT);
  if (slot2)
    {
      /* NAME is already there.  */
      return NULL;
    }

  slot1 = htab_find_slot_with_hash (groups_id, &id, GROUP_ID_HASH (id), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1)
    abort ();
#endif
  slot2 = htab_find_slot_with_hash (groups_name, name, GROUP_NAME_HASH (name),
				    INSERT);
#ifdef ENABLE_CHECKING
  if (!slot2)
    abort ();
#endif

  g = (group) xmalloc (sizeof (*g));
  g->id = id;
  g->name = xstrdup (name);
  *slot1 = g;
  *slot2 = g;

  /* Add the group to user's list, for each user in groups list.  */
  if (user_list && *user_list)
    {
      char *name;

      while (*user_list)
	{
	  while (*user_list == ',')
	    user_list++;

	  name = user_list;
	  while ((*user_list >= 'A' && *user_list <= 'Z')
		 || (*user_list >= 'a' && *user_list <= 'z')
		 || (*user_list >= '0' && *user_list <= '9')
		 || *user_list == '_' || *user_list == '-')
	    user_list++;
	  
	  if (*user_list == ',' || *user_list == 0)
	    {
	      user u;

	      if (*user_list == ',')
		*user_list++ = 0;

	      u = (user) htab_find_with_hash (users_name, name,
					      USER_NAME_HASH (name));
	      if (!u)
		{
		  message (1, stderr, "Unknown user: %s\n", name);
		}
	      else
		{
		  void **slot;

		  slot = htab_find_slot_with_hash (u->groups, &id,
						   GROUP_ID_HASH (id),
						   INSERT);
		  if (!*slot)
		    *slot = g;
		}
	    }
	  else
	    {
	      while (*user_list != ',' && *user_list != 0)
		user_list++;
	      if (*user_list == ',')
		*user_list++ = 0;

	      message (1, stderr, "User name contains illegal chars: %s\n",
		       name);
	    }
	}
      
    }

  return g;
}

/* Destroy group G.  */

void
group_destroy (group g)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot = htab_find_slot_with_hash (groups_id, &g->id, GROUP_ID_HASH (g->id),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (groups_id, slot);

  slot = htab_find_slot_with_hash (groups_name, g->name,
				   GROUP_NAME_HASH (g->name), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (groups_name, slot);

  free (g->name);
  free (g);
}

/* Initialize data structures in USER-GROUP.C.  */

void
initialize_user_group_c ()
{
  zfsd_mutex_init (&users_groups_mutex);

  users_id = htab_create (100, users_id_hash, users_id_eq, NULL,
			  &users_groups_mutex);
  users_name = htab_create (100, users_name_hash, users_name_eq, NULL,
			    &users_groups_mutex);
  groups_id = htab_create (100, groups_id_hash, groups_id_eq, NULL,
			   &users_groups_mutex);
  groups_name = htab_create (100, groups_name_hash, groups_name_eq, NULL,
			     &users_groups_mutex);
}

/* Destroy data structures in USER-GROUP.C.  */

void
cleanup_user_group_c ()
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (users_id, slot,
    {
      user u = (user) *slot;

      user_destroy (u);
    });
  htab_destroy (users_id);
  htab_destroy (users_name);

  HTAB_FOR_EACH_SLOT (users_id, slot,
    {
      group g = (group) *slot;

      group_destroy (g);
    });
  htab_destroy (groups_id);
  htab_destroy (groups_name);

  zfsd_mutex_unlock (&users_groups_mutex);
  zfsd_mutex_destroy (&users_groups_mutex);
}
