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
#include <inttypes.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "pthread.h"
#include "log.h"
#include "memory.h"
#include "user-group.h"
#include "hashtab.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "node.h"

/* Tables of users and groups, searched by ID and by NAME.  */
static htab_t users_id;
static htab_t users_name;
static htab_t groups_id;
static htab_t groups_name;

/* Tables for mapping between ZFS IDs and node IDs.  */
static htab_t map_uid_to_node;
static htab_t map_uid_to_zfs;
static htab_t map_gid_to_node;

/* Mutex protecting hash tables users_*, groups_*, map_*.  */
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
  return USER_ID_HASH (((user_t) x)->id);
}

/* Hash function for user X, computed from NAME.  */

static hash_t
users_name_hash (const void *x)
{
  return USER_NAME_HASH (((user_t) x)->name);
}

/* Compare an user X with user ID Y.  */

static int
users_id_eq (const void *x, const void *y)
{
  return ((user_t) x)->id == *(uint32_t *) y;
}

/* Compare an user X with user name Y.  */

static int
users_name_eq (const void *x, const void *y)
{
  return strcmp (((user_t) x)->name, (const char *) y);
}

/* Hash function for group X, computed from ID.  */

static hash_t
groups_id_hash (const void *x)
{
  return GROUP_ID_HASH (((group_t) x)->id);
}

/* Hash function for group X, computed from NAME.  */

static hash_t
groups_name_hash (const void *x)
{
  return GROUP_NAME_HASH (((group_t) x)->name);
}

/* Compare a group X with group ID Y.  */

static int
groups_id_eq (const void *x, const void *y)
{
  return ((group_t) x)->id == *(uint32_t *) y;
}

/* Compare a group X with group name Y.  */

static int
groups_name_eq (const void *x, const void *y)
{
  return strcmp (((group_t) x)->name, (const char *) y);
}

/* Create an user with ID and NAME with default group GID.  */

user_t
user_create (uint32_t id, char *name, uint32_t gid)
{
  user_t u;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (users_id, &id, USER_ID_HASH (id),
				    NO_INSERT);
  if (slot1)
    {
      /* ID is already there.  */
      message (1, stderr, "Duplicate user ID: %" PRIu32 "\n", id);
      return NULL;
    }
  slot2 = htab_find_slot_with_hash (users_name, name, USER_NAME_HASH (name),
				    NO_INSERT);
  if (slot2)
    {
      /* NAME is already there.  */
      message (1, stderr, "Duplicate user name: %s\n", name);
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

  u = (user_t) xmalloc (sizeof (*u));
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
      user_t u = (user_t) slot;
      group_t g;

      g = (group_t) htab_find_with_hash (groups_id, &u->gid,
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
user_destroy (user_t u)
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

group_t
group_create (uint32_t id, char *name, char *user_list)
{
  group_t g;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (groups_id, &id, GROUP_ID_HASH (id),
				    NO_INSERT);
  if (slot1)
    {
      /* ID is already there.  */
      message (1, stderr, "Duplicate group ID: %" PRIu32 "\n", id);
      return NULL;
    }
  slot2 = htab_find_slot_with_hash (groups_name, name, GROUP_NAME_HASH (name),
				    NO_INSERT);
  if (slot2)
    {
      /* NAME is already there.  */
      message (1, stderr, "Duplicate group name: %s\n", name);
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

  g = (group_t) xmalloc (sizeof (*g));
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
	      user_t u;

	      if (*user_list == ',')
		*user_list++ = 0;

	      u = (user_t) htab_find_with_hash (users_name, name,
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
group_destroy (group_t g)
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

/* Hash function for id_mapping, computed from ZFS_ID.  */

hash_t
map_id_to_node_hash (const void *x)
{
  return MAP_ID_HASH (((id_mapping) x)->zfs_id);
}

/* Hash function for id_mapping, computed from NODE_ID.  */

hash_t
map_id_to_zfs_hash (const void *x)
{
  return MAP_ID_HASH (((id_mapping) x)->node_id);
}

/* Compare ID mapping X with ZFS user/group ID Y.  */

int
map_id_to_node_eq (const void *x, const void *y)
{
  return ((id_mapping) x)->zfs_id == *(uint32_t *) y;
}

/* Compare ID mapping X with node user/group ID Y.  */

int
map_id_to_zfs_eq (const void *x, const void *y)
{
  return ((id_mapping) x)->node_id == *(uint32_t *) y;
}

/* Add mapping between ZFS user name ZFS_USER and node user name NODE_USER
   for node NOD. If NOD is NULL add it to default mapping.  */

id_mapping
user_mapping_create (char *zfs_user, char *node_user, node nod)
{
  user_t u;
  struct passwd *pwd;
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot1;
  void **slot2;
  id_mapping map;

  u = (user_t) htab_find_with_hash (users_name, zfs_user,
				    USER_NAME_HASH (zfs_user));
  if (!u)
    {
      message (1, stderr,
	       "ZFS user '%s' for mapping '%s'<->'%s' does not exist\n",
	       zfs_user, zfs_user, node_user);
      return NULL;
    }

  pwd = getpwnam (node_user);
  if (!pwd)
    {
      message (1, stderr,
	       "Node user '%s' for mapping '%s'<->'%s' does not exist\n",
	       node_user, zfs_user, node_user);
      return NULL;
    }
  
  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_uid_to_node || !nod->map_uid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_uid_to_node;
      map_to_zfs = nod->map_uid_to_zfs;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_uid_to_node;
      map_to_zfs = map_uid_to_zfs;
    }

  slot1 = htab_find_slot_with_hash (map_to_node, &u->id, USER_ID_HASH (u->id),
				    INSERT);
  slot2 = htab_find_slot_with_hash (map_to_zfs, &pwd->pw_uid,
				    USER_ID_HASH (pwd->pw_uid), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1 || !slot2)
    abort ();
#endif

  /* If both UIDs are in the tables we have nothing to do.  */
  if (*slot1 && *slot2)
    return (id_mapping) *slot1;

  map = (id_mapping) xmalloc (sizeof (*map));
  map->zfs_id = u->id;
  map->node_id = pwd->pw_uid;

  if (!*slot1)
    *slot1 = map;
  if (!*slot2)
    *slot2 = map;

  return map;
}

/* Destroy ID mapping MAP for node NOD.  If NOD is NULL delete it from
   default mapping.  */

static void
user_mapping_destroy (id_mapping map, node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_uid_to_node || !nod->map_uid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_uid_to_node;
      map_to_zfs = nod->map_uid_to_zfs;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_uid_to_node;
      map_to_zfs = map_uid_to_zfs;
    }

  slot = htab_find_slot_with_hash (map_to_node, &map->zfs_id,
				   USER_ID_HASH (map->zfs_id), NO_INSERT);
  if (slot)
    htab_clear_slot (map_to_node, slot);

  slot = htab_find_slot_with_hash (map_to_zfs, &map->node_id,
				   USER_ID_HASH (map->node_id), NO_INSERT);
  if (slot)
    htab_clear_slot (map_to_zfs, slot);

  free (map);
}

/* Destroy all user mappings between ZFS and node NOD.
   If NOD is NULL destroy default mapping.  */

void
user_mapping_destroy_all (node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_uid_to_node || !nod->map_uid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_uid_to_node;
      map_to_zfs = nod->map_uid_to_zfs;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_uid_to_node;
      map_to_zfs = map_uid_to_zfs;
    }

  HTAB_FOR_EACH_SLOT (map_to_node, slot,
    {
      id_mapping map = (id_mapping) *slot;

      user_mapping_destroy (map, nod);
    });
  HTAB_FOR_EACH_SLOT (map_to_zfs, slot,
    {
      id_mapping map = (id_mapping) *slot;

      user_mapping_destroy (map, nod);
    });
}

/* Add mapping between ZFS group name ZFS_GROUP and node group name NODE_GROUP
   for node NOD. If NOD is NULL add it to default mapping.  */

id_mapping
group_mapping_create (char *zfs_group, char *node_group, node nod)
{
  group_t g;
  struct group *grp;
  htab_t map_to_node;
  void **slot;
  id_mapping map;

  g = (group_t) htab_find_with_hash (groups_name, zfs_group,
				     GROUP_NAME_HASH (zfs_group));
  if (!g)
    {
      message (1, stderr,
	       "ZFS group '%s' for mapping '%s'<->'%s' does not exist\n",
	       zfs_group, zfs_group, node_group);
      return NULL;
    }

  grp = getgrnam (node_group);
  if (!grp)
    {
      message (1, stderr,
	       "Node group '%s' for mapping '%s'<->'%s' does not exist\n",
	       node_group, zfs_group, node_group);
      return NULL;
    }

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_uid_to_node;
    }

  slot = htab_find_slot_with_hash (map_to_node, &g->id, GROUP_ID_HASH (g->id),
				   INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif

  /* If G->ID is already in the table we have nothing to do.  */
  if (*slot)
    return (id_mapping) *slot;

  map = (id_mapping) xmalloc (sizeof (*map));
  map->zfs_id = g->id;
  map->node_id = grp->gr_gid;
  *slot = map;

  return map;
}

/* Destroy ID mapping MAP for node NOD.  If NOD is NULL delete it from
   default mapping.  */

static void
group_mapping_destroy (id_mapping map, node nod)
{
  htab_t map_to_node;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_gid_to_node;
    }

  slot = htab_find_slot_with_hash (map_to_node, &map->zfs_id,
				   GROUP_ID_HASH (map->zfs_id), NO_INSERT);
  if (slot)
    htab_clear_slot (map_to_node, slot);

  free (map);
}

/* Destroy all group mappings between ZFS and node NOD.
   If NOD is NULL destroy default mapping.  */

void
group_mapping_destroy_all (node nod)
{
  htab_t map_to_node;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_gid_to_node;
    }

  HTAB_FOR_EACH_SLOT (map_to_node, slot,
    {
      id_mapping map = (id_mapping) *slot;

      group_mapping_destroy (map, nod);
    });
}

/* Initialize data structures in USER-GROUP.C.  */

void
initialize_user_group_c ()
{
  zfsd_mutex_init (&users_groups_mutex);

  /* User and group tables.  */
  users_id = htab_create (100, users_id_hash, users_id_eq, NULL,
			  &users_groups_mutex);
  users_name = htab_create (100, users_name_hash, users_name_eq, NULL,
			    &users_groups_mutex);
  groups_id = htab_create (100, groups_id_hash, groups_id_eq, NULL,
			   &users_groups_mutex);
  groups_name = htab_create (100, groups_name_hash, groups_name_eq, NULL,
			     &users_groups_mutex);

  /* Tables for mapping user and group IDs between ZFS and node.  */
  map_uid_to_node = htab_create (20, map_id_to_node_hash, map_id_to_node_eq,
				 NULL, &users_groups_mutex);
  map_uid_to_zfs = htab_create (20, map_id_to_zfs_hash, map_id_to_zfs_eq,
				NULL, &users_groups_mutex);
  map_gid_to_node = htab_create (20, map_id_to_node_hash, map_id_to_node_eq,
				 NULL, &users_groups_mutex);
}

/* Destroy data structures in USER-GROUP.C.  */

void
cleanup_user_group_c ()
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);

  /* User and group tables.  */
  HTAB_FOR_EACH_SLOT (users_id, slot,
    {
      user_t u = (user_t) *slot;

      user_destroy (u);
    });
  htab_destroy (users_id);
  htab_destroy (users_name);

  HTAB_FOR_EACH_SLOT (users_id, slot,
    {
      group_t g = (group_t) *slot;

      group_destroy (g);
    });
  htab_destroy (groups_id);
  htab_destroy (groups_name);

  /* Tables for mapping user and group IDs between ZFS and node.  */
  user_mapping_destroy_all (NULL);
  htab_destroy (map_uid_to_node);
  htab_destroy (map_uid_to_zfs);
  group_mapping_destroy_all (NULL);
  htab_destroy (map_gid_to_node);

  zfsd_mutex_unlock (&users_groups_mutex);
  zfsd_mutex_destroy (&users_groups_mutex);
}