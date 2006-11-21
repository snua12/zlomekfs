/*! \file
    \brief User and group functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

/*! Tables of users and groups, searched by ID and by NAME.  */
static htab_t users_id;
static htab_t users_name;
static htab_t groups_id;
static htab_t groups_name;

/*! Tables for mapping between ZFS IDs and node IDs.  */
static htab_t map_uid_to_node;
static htab_t map_uid_to_zfs;
static htab_t map_gid_to_node;
static htab_t map_gid_to_zfs;

/*! Mutex protecting hash tables users_*, groups_*, map_*.  */
pthread_mutex_t users_groups_mutex;

/*! ID of default node user/group.  */
uint32_t default_node_uid = (uint32_t) -1;
uint32_t default_node_gid = (uint32_t) -1;

/*! Hash functions for user and group ID.  */
#define USER_ID_HASH(ID) (ID)
#define GROUP_ID_HASH(ID) (ID)

/*! Hash functions for user and group NAME.  */
#define USER_NAME_HASH(NAME) (crc32_buffer ((NAME).str, (NAME).len))
#define GROUP_NAME_HASH(NAME) (crc32_buffer ((NAME).str, (NAME).len))

/*! Hash function for user X, computed from ID.  */

static hash_t
users_id_hash (const void *x)
{
  return USER_ID_HASH (((const struct user_def *) x)->id);
}

/*! Hash function for user X, computed from NAME.  */

static hash_t
users_name_hash (const void *x)
{
  return USER_NAME_HASH (((const struct user_def *) x)->name);
}

/*! Compare an user X with user ID Y.  */

static int
users_id_eq (const void *x, const void *y)
{
  return ((const struct user_def *) x)->id == *(const uint32_t *) y;
}

/*! Compare an user X with user name Y.  */

static int
users_name_eq (const void *x, const void *y)
{
  const struct user_def *u = (const struct user_def *) x;
  const string *s = (const string *) y;

  return (u->name.len == s->len
	  && strcmp (u->name.str, s->str) == 0);
}

/*! Hash function for group X, computed from ID.  */

static hash_t
groups_id_hash (const void *x)
{
  return GROUP_ID_HASH (((const struct group_def *) x)->id);
}

/*! Hash function for group X, computed from NAME.  */

static hash_t
groups_name_hash (const void *x)
{
  return GROUP_NAME_HASH (((const struct group_def *) x)->name);
}

/*! Compare a group X with group ID Y.  */

static int
groups_id_eq (const void *x, const void *y)
{
  return ((const struct group_def *) x)->id == *(const uint32_t *) y;
}

/*! Compare a group X with group name Y.  */

static int
groups_name_eq (const void *x, const void *y)
{
  const struct group_def *g = (const struct group_def *) x;
  const string *s = (const string *) y;

  return (g->name.len == s->len
	  && strcmp (g->name.str, s->str) == 0);
}

/*! Create an user with ID and NAME.  */

user_t
user_create (uint32_t id, string *name)
{
  user_t u;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (users_id, &id, USER_ID_HASH (id),
				    NO_INSERT);
  slot2 = htab_find_slot_with_hash (users_name, name, USER_NAME_HASH (*name),
				    NO_INSERT);
  if (slot1 && slot2 && *slot1 == *slot2)
    {
      u = (user_t) *slot1;
      if (!u->marked)
	{
	  message (1, stderr, "Duplicate user ID and name: %" PRIu32 " %s\n",
		   id, name->str);
	  return NULL;
	}
      u->marked = false;
      return u;
    }
  if (slot1)
    {
      /* ID is already there.  */
      message (1, stderr, "Duplicate user ID: %" PRIu32 "\n", id);
      return NULL;
    }
  if (slot2)
    {
      /* NAME is already there.  */
      message (1, stderr, "Duplicate user name: %s\n", name->str);
      return NULL;
    }

  slot1 = htab_find_slot_with_hash (users_id, &id, USER_ID_HASH (id), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1)
    abort ();
#endif
  slot2 = htab_find_slot_with_hash (users_name, name, USER_NAME_HASH (*name),
				    INSERT);
#ifdef ENABLE_CHECKING
  if (!slot2)
    abort ();
#endif

  u = (user_t) xmalloc (sizeof (*u));
  u->id = id;
  xstringdup (&u->name, name);
  u->marked = false;
  *slot1 = u;
  *slot2 = u;

  return u;
}

/*! Lookup user by ID.  */

static user_t
user_lookup (uint32_t id)
{
  return (user_t) htab_find_with_hash (users_id, &id, USER_ID_HASH (id));
}

/*! Destroy user U.  */

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

  slot = htab_find_slot_with_hash (users_name, &u->name,
				   USER_NAME_HASH (u->name), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (users_name, slot);

  free (u->name.str);
  free (u);
}

/*! Create a group with ID and NAME, its list of users is USER_LIST.  */

group_t
group_create (uint32_t id, string *name)
{
  group_t g;
  void **slot1;
  void **slot2;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  slot1 = htab_find_slot_with_hash (groups_id, &id, GROUP_ID_HASH (id),
				    NO_INSERT);
  slot2 = htab_find_slot_with_hash (groups_name, name, GROUP_NAME_HASH (*name),
				    NO_INSERT);
  if (slot1 && slot2 && *slot1 == *slot2)
    {
      g = (group_t) *slot1;
      if (!g->marked)
	{
	  message (1, stderr, "Duplicate group ID and name: %" PRIu32 " %s\n",
		   id, name->str);
	  return NULL;
	}
      g->marked = false;
      return g;
    }
  if (slot1)
    {
      /* ID is already there.  */
      message (1, stderr, "Duplicate group ID: %" PRIu32 "\n", id);
      return NULL;
    }
  if (slot2)
    {
      /* NAME is already there.  */
      message (1, stderr, "Duplicate group name: %s\n", name->str);
      return NULL;
    }

  slot1 = htab_find_slot_with_hash (groups_id, &id, GROUP_ID_HASH (id), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1)
    abort ();
#endif
  slot2 = htab_find_slot_with_hash (groups_name, name, GROUP_NAME_HASH (*name),
				    INSERT);
#ifdef ENABLE_CHECKING
  if (!slot2)
    abort ();
#endif

  g = (group_t) xmalloc (sizeof (*g));
  g->id = id;
  xstringdup (&g->name, name);
  g->marked = false;
  *slot1 = g;
  *slot2 = g;

  return g;
}

/*! Lookup group by ID.  */

static group_t
group_lookup (uint32_t id)
{
  return (group_t) htab_find_with_hash (groups_id, &id, GROUP_ID_HASH (id));
}

/*! Destroy group G.  */

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

  slot = htab_find_slot_with_hash (groups_name, &g->name,
				   GROUP_NAME_HASH (g->name), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (groups_name, slot);

  free (g->name.str);
  free (g);
}

/*! Hash function for id_mapping, computed from ZFS_ID.  */

hash_t
map_id_to_node_hash (const void *x)
{
  return MAP_ID_HASH (((const struct id_mapping_def *) x)->zfs_id);
}

/*! Hash function for id_mapping, computed from NODE_ID.  */

hash_t
map_id_to_zfs_hash (const void *x)
{
  return MAP_ID_HASH (((const struct id_mapping_def *) x)->node_id);
}

/*! Compare ID mapping X with ZFS user/group ID Y.  */

int
map_id_to_node_eq (const void *x, const void *y)
{
  return ((const struct id_mapping_def *) x)->zfs_id == *(const uint32_t *) y;
}

/*! Compare ID mapping X with node user/group ID Y.  */

int
map_id_to_zfs_eq (const void *x, const void *y)
{
  return ((const struct id_mapping_def *) x)->node_id == *(const uint32_t *) y;
}

/*! Add mapping between ZFS user name ZFS_USER and node user name NODE_USER
   for node NOD. If NOD is NULL add it to default mapping.  */

id_mapping
user_mapping_create (string *zfs_user, string *node_user, node nod)
{
  user_t u;
  struct passwd *pwd;
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot1;
  void **slot2;
  id_mapping map;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  u = (user_t) htab_find_with_hash (users_name, zfs_user,
				    USER_NAME_HASH (*zfs_user));
  if (!u || u->marked)
    {
      message (1, stderr,
	       "ZFS user '%s' for mapping '%s'<->'%s' does not exist\n",
	       zfs_user->str, zfs_user->str, node_user->str);
      return NULL;
    }

  pwd = getpwnam (node_user->str);
  if (!pwd)
    {
      message (1, stderr,
	       "Node user '%s' for mapping '%s'<->'%s' does not exist\n",
	       node_user->str, zfs_user->str, node_user->str);
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
  map->marked = false;

  if (!*slot1)
    *slot1 = map;
  if (!*slot2)
    *slot2 = map;

  return map;
}

/*! Destroy ID mapping MAP for node NOD.  If NOD is NULL delete it from
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

/*! For each ZFS user try to create mapping "user" <-> "user".  */

void
set_default_user_mapping (void)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  HTAB_FOR_EACH_SLOT (users_id, slot)
    {
      user_t u = (user_t) slot;

      user_mapping_create (&u->name, &u->name, NULL);
    }
}

/*! Destroy all user mappings between ZFS and node NOD.
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

  HTAB_FOR_EACH_SLOT (map_to_node, slot)
    {
      id_mapping map = (id_mapping) *slot;

      user_mapping_destroy (map, nod);
    }
  HTAB_FOR_EACH_SLOT (map_to_zfs, slot)
    {
      id_mapping map = (id_mapping) *slot;

      user_mapping_destroy (map, nod);
    }
}

/*! Add mapping between ZFS group name ZFS_GROUP and node group name NODE_GROUP
   for node NOD. If NOD is NULL add it to default mapping.  */

id_mapping
group_mapping_create (string *zfs_group, string *node_group, node nod)
{
  group_t g;
  struct group *grp;
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot1;
  void **slot2;
  id_mapping map;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  g = (group_t) htab_find_with_hash (groups_name, zfs_group,
				     GROUP_NAME_HASH (*zfs_group));
  if (!g || g->marked)
    {
      message (1, stderr,
	       "ZFS group '%s' for mapping '%s'<->'%s' does not exist\n",
	       zfs_group->str, zfs_group->str, node_group->str);
      return NULL;
    }

  grp = getgrnam (node_group->str);
  if (!grp)
    {
      message (1, stderr,
	       "Node group '%s' for mapping '%s'<->'%s' does not exist\n",
	       node_group->str, zfs_group->str, node_group->str);
      return NULL;
    }

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node || !nod->map_gid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
      map_to_zfs = nod->map_gid_to_zfs;
    }
  else
    {
      map_to_node = map_uid_to_node;
      map_to_zfs = map_gid_to_zfs;
    }

  slot1 = htab_find_slot_with_hash (map_to_node, &g->id, GROUP_ID_HASH (g->id),
				    INSERT);
  slot2 = htab_find_slot_with_hash (map_to_zfs, &grp->gr_gid,
				    GROUP_ID_HASH (grp->gr_gid), INSERT);
#ifdef ENABLE_CHECKING
  if (!slot1 || !slot2)
    abort ();
#endif

  /* If both GIDs are in the tables we have nothing to do.  */
  if (*slot1 && *slot2)
    return (id_mapping) *slot1;

  map = (id_mapping) xmalloc (sizeof (*map));
  map->zfs_id = g->id;
  map->node_id = grp->gr_gid;
  map->marked = false;

  if (!*slot1)
    *slot1 = map;
  if (!*slot2)
    *slot2 = map;

  return map;
}

/*! Destroy ID mapping MAP for node NOD.  If NOD is NULL delete it from
   default mapping.  */

static void
group_mapping_destroy (id_mapping map, node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node || !nod->map_gid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
      map_to_zfs = nod->map_gid_to_zfs;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_gid_to_node;
      map_to_zfs = map_gid_to_zfs;
    }

  slot = htab_find_slot_with_hash (map_to_node, &map->zfs_id,
				   GROUP_ID_HASH (map->zfs_id), NO_INSERT);
  if (slot)
    htab_clear_slot (map_to_node, slot);

  slot = htab_find_slot_with_hash (map_to_zfs, &map->node_id,
				   GROUP_ID_HASH (map->node_id), NO_INSERT);
  if (slot)
    htab_clear_slot (map_to_zfs, slot);

  free (map);
}

/*! For each ZFS group try to create mapping "group" <-> "group".  */

void
set_default_group_mapping (void)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&users_groups_mutex);

  HTAB_FOR_EACH_SLOT (groups_id, slot)
    {
      group_t g = (group_t) slot;

      group_mapping_create (&g->name, &g->name, NULL);
    }
}

/*! Destroy all group mappings between ZFS and node NOD.
   If NOD is NULL destroy default mapping.  */

void
group_mapping_destroy_all (node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node || !nod->map_gid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
      map_to_zfs = nod->map_gid_to_zfs;
    }
  else
    {
      CHECK_MUTEX_LOCKED (&users_groups_mutex);

      map_to_node = map_gid_to_node;
      map_to_zfs = map_gid_to_zfs;
    }

  HTAB_FOR_EACH_SLOT (map_to_node, slot)
    {
      id_mapping map = (id_mapping) *slot;

      group_mapping_destroy (map, nod);
    }
  HTAB_FOR_EACH_SLOT (map_to_zfs, slot)
    {
      id_mapping map = (id_mapping) *slot;

      group_mapping_destroy (map, nod);
    }
}

/*! Map ZFS user UID to (local) node user ID.  */

uint32_t
map_uid_zfs2node (uint32_t uid)
{
  id_mapping map;

  if (uid == (uint32_t) -1)
    return uid;

  zfsd_mutex_lock (&this_node->mutex);
  map = (id_mapping) htab_find_with_hash (this_node->map_uid_to_node, &uid,
					  USER_ID_HASH (uid));
  if (map)
    {
      zfsd_mutex_unlock (&this_node->mutex);
      return map->node_id;
    }
  zfsd_mutex_unlock (&this_node->mutex);

  zfsd_mutex_lock (&users_groups_mutex);
  map = (id_mapping) htab_find_with_hash (map_uid_to_node, &uid,
					  USER_ID_HASH (uid));
  if (map)
    {
      zfsd_mutex_unlock (&users_groups_mutex);
      return map->node_id;
    }
  zfsd_mutex_unlock (&users_groups_mutex);

  return default_node_uid;
}

/*! Map (local) node UID to ZFS user ID.  */

uint32_t
map_uid_node2zfs (uint32_t uid)
{
  id_mapping map;

  if (uid == (uint32_t) -1)
    return uid;

  zfsd_mutex_lock (&this_node->mutex);
  map = (id_mapping) htab_find_with_hash (this_node->map_uid_to_zfs, &uid,
					  USER_ID_HASH (uid));
  if (map)
    {
      zfsd_mutex_unlock (&this_node->mutex);
      return map->zfs_id;
    }
  zfsd_mutex_unlock (&this_node->mutex);

  zfsd_mutex_lock (&users_groups_mutex);
  map = (id_mapping) htab_find_with_hash (map_uid_to_zfs, &uid,
					  USER_ID_HASH (uid));
  if (map)
    {
      zfsd_mutex_unlock (&users_groups_mutex);
      return map->zfs_id;
    }
  zfsd_mutex_unlock (&users_groups_mutex);

  return DEFAULT_ZFS_UID;
}

/*! Map ZFS group GID to (local) node group ID.  */

uint32_t
map_gid_zfs2node (uint32_t gid)
{
  id_mapping map;

  if (gid == (uint32_t) -1)
    return gid;

  zfsd_mutex_lock (&this_node->mutex);
  map = (id_mapping) htab_find_with_hash (this_node->map_gid_to_node, &gid,
					  GROUP_ID_HASH (gid));
  if (map)
    {
      zfsd_mutex_unlock (&this_node->mutex);
      return map->node_id;
    }
  zfsd_mutex_unlock (&this_node->mutex);

  zfsd_mutex_lock (&users_groups_mutex);
  map = (id_mapping) htab_find_with_hash (map_gid_to_node, &gid,
					  GROUP_ID_HASH (gid));
  if (map)
    {
      zfsd_mutex_unlock (&users_groups_mutex);
      return map->node_id;
    }
  zfsd_mutex_unlock (&users_groups_mutex);

  return default_node_gid;
}

/*! Map (local) node GID to ZFS group ID.  */

uint32_t
map_gid_node2zfs (uint32_t gid)
{
  id_mapping map;

  if (gid == (uint32_t) -1)
    return gid;

  zfsd_mutex_lock (&this_node->mutex);
  map = (id_mapping) htab_find_with_hash (this_node->map_gid_to_zfs, &gid,
					  GROUP_ID_HASH (gid));
  if (map)
    {
      zfsd_mutex_unlock (&this_node->mutex);
      return map->zfs_id;
    }
  zfsd_mutex_unlock (&this_node->mutex);

  zfsd_mutex_lock (&users_groups_mutex);
  map = (id_mapping) htab_find_with_hash (map_gid_to_zfs, &gid,
					  GROUP_ID_HASH (gid));
  if (map)
    {
      zfsd_mutex_unlock (&users_groups_mutex);
      return map->zfs_id;
    }
  zfsd_mutex_unlock (&users_groups_mutex);

  return DEFAULT_ZFS_GID;
}

/*! Mark all users.  */

void
mark_all_users (void)
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (users_id, slot)
    {
      user_t u = (user_t) *slot;

      u->marked = true;
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Mark all groups.  */

void
mark_all_groups (void)
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (groups_id, slot)
    {
      group_t g = (group_t) *slot;

      g->marked = true;
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Mark all id mappings in table HTAB.  */

static void
mark_id_mapping (htab_t htab)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (htab, slot)
    {
      id_mapping map = (id_mapping) *slot;

      map->marked = true;
    }
}

/*! Mark user mapping.  If NOD is defined mark user mapping for node NOD
   otherwise mark the global user mapping.  */

void
mark_user_mapping (node nod)
{
  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);

      mark_id_mapping (nod->map_uid_to_node);
    }
  else
    {
      zfsd_mutex_lock (&users_groups_mutex);
      mark_id_mapping (map_uid_to_node);
      zfsd_mutex_unlock (&users_groups_mutex);
    }
}

/*! Mark group mapping.  If NOD is defined mark group mapping for node NOD
   otherwise mark the global group mapping.  */

void
mark_group_mapping (node nod)
{
  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);

      mark_id_mapping (nod->map_gid_to_node);
    }
  else
    {
      zfsd_mutex_lock (&users_groups_mutex);
      mark_id_mapping (map_gid_to_node);
      zfsd_mutex_unlock (&users_groups_mutex);
    }
}

/*! Destroy marked users.  */

void
destroy_marked_users (void)
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (users_id, slot)
    {
      user_t u = (user_t) *slot;

      if (u->marked)
	user_destroy (u);
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Destroy marked groups.  */

void
destroy_marked_groups (void)
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (groups_id, slot)
    {
      group_t g = (group_t) *slot;

      if (g->marked)
	group_destroy (g);
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Destroy marked user mapping.  */

void
destroy_marked_user_mapping (node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;
  user_t u;

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
      map_to_node = map_uid_to_node;
      map_to_zfs = map_uid_to_zfs;
    }

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (map_to_node, slot)
    {
      id_mapping map = (id_mapping) *slot;

      if (map->marked)
	user_mapping_destroy (map, nod);
      else
	{
	  u = user_lookup (map->zfs_id);
	  if (!u || u->marked)
	    user_mapping_destroy (map, nod);
	}
    }
  HTAB_FOR_EACH_SLOT (map_to_zfs, slot)
    {
      id_mapping map = (id_mapping) *slot;

      if (map->marked)
	user_mapping_destroy (map, nod);
      else
	{
	  u = user_lookup (map->zfs_id);
	  if (!u || u->marked)
	    user_mapping_destroy (map, nod);
	}
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Destroy marked group mapping.  */

void
destroy_marked_group_mapping (node nod)
{
  htab_t map_to_node;
  htab_t map_to_zfs;
  void **slot;
  group_t g;

  if (nod)
    {
      CHECK_MUTEX_LOCKED (&nod->mutex);
#ifdef ENABLE_CHECKING
      if (!nod->map_gid_to_node || !nod->map_gid_to_zfs)
	abort ();
#endif

      map_to_node = nod->map_gid_to_node;
      map_to_zfs = nod->map_gid_to_zfs;
    }
  else
    {
      map_to_node = map_gid_to_node;
      map_to_zfs = map_gid_to_zfs;
    }

  zfsd_mutex_lock (&users_groups_mutex);
  HTAB_FOR_EACH_SLOT (map_to_node, slot)
    {
      id_mapping map = (id_mapping) *slot;

      if (map->marked)
	group_mapping_destroy (map, nod);
      else
	{
	  g = group_lookup (map->zfs_id);
	  if (!g || g->marked)
	    group_mapping_destroy (map, nod);
	}
    }
  HTAB_FOR_EACH_SLOT (map_to_zfs, slot)
    {
      id_mapping map = (id_mapping) *slot;

      if (map->marked)
	group_mapping_destroy (map, nod);
      else
	{
	  g = group_lookup (map->zfs_id);
	  if (!g || g->marked)
	    group_mapping_destroy (map, nod);
	}
    }
  zfsd_mutex_unlock (&users_groups_mutex);
}

/*! Initialize data structures in USER-GROUP.C.  */

void
initialize_user_group_c (void)
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
  map_gid_to_zfs = htab_create (20, map_id_to_zfs_hash, map_id_to_zfs_eq,
				NULL, &users_groups_mutex);
}

/*! Destroy data structures in USER-GROUP.C.  */

void
cleanup_user_group_c (void)
{
  void **slot;

  zfsd_mutex_lock (&users_groups_mutex);

  /* Tables for mapping user and group IDs between ZFS and node.  */
  user_mapping_destroy_all (NULL);
  htab_destroy (map_uid_to_node);
  htab_destroy (map_uid_to_zfs);

  group_mapping_destroy_all (NULL);
  htab_destroy (map_gid_to_node);
  htab_destroy (map_gid_to_zfs);

  /* User and group tables.  */
  HTAB_FOR_EACH_SLOT (users_id, slot)
    {
      user_t u = (user_t) *slot;

      user_destroy (u);
    }
  htab_destroy (users_id);
  htab_destroy (users_name);

  HTAB_FOR_EACH_SLOT (groups_id, slot)
    {
      group_t g = (group_t) *slot;

      group_destroy (g);
    }
  htab_destroy (groups_id);
  htab_destroy (groups_name);

  zfsd_mutex_unlock (&users_groups_mutex);
  zfsd_mutex_destroy (&users_groups_mutex);
}
