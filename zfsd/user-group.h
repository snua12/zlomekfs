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

#ifndef USER_GROUP_H
#define USER_GROUP_H

#include "system.h"
#include <inttypes.h>
#include "hashtab.h"
#include "node.h"

/*! Description of ZFS user.  */
typedef struct user_def
{
  uint32_t id;			/*!< user ID */
  string name;			/*!< name of the user */
  bool marked;			/*!< Is the user marked?  */
} *user_t;

/*! Description of ZFS group.  */
typedef struct group_def
{
  uint32_t id;			/*!< group ID */
  string name;			/*!< name of the group */
  bool marked;			/*!< Is the group marked?  */
} *group_t;

/*! Structure describing mapping between ZFS user/group ID
   and node user/group ID.  */
typedef struct id_mapping_def
{
  uint32_t zfs_id;		/*!< ID of ZFS user/group */
  uint32_t node_id;		/*!< ID of node user/group */
  bool marked;			/*!< Is the id mapping marked?  */
} *id_mapping;

/*! ID of default ZFS user/group.  */
#define DEFAULT_ZFS_UID ((uint32_t) -2)
#define DEFAULT_ZFS_GID ((uint32_t) -2)

/*! Mutex protecting hash tables users_*, groups_*, map_*.  */
extern pthread_mutex_t users_groups_mutex;

/*! ID of default node user/group.  */
extern uint32_t default_node_uid;
extern uint32_t default_node_gid;

/*! Hash functions for user/group ID mapping.  */
#define MAP_ID_HASH(UID) (UID)

extern user_t user_create (uint32_t id, string *name);
extern void user_destroy (user_t u);
extern group_t group_create (uint32_t id, string *name);
extern void group_destroy (group_t g);

extern hash_t map_id_to_node_hash (const void *x);
extern hash_t map_id_to_zfs_hash (const void *x);
extern int map_id_to_node_eq (const void *x, const void *y);
extern int map_id_to_zfs_eq (const void *x, const void *y);
extern id_mapping user_mapping_create (string *zfs_user, string *node_user,
				       node nod);
extern void set_default_user_mapping (void);
extern void user_mapping_destroy_all (node nod);
extern id_mapping group_mapping_create (string *zfs_group, string *node_group,
					node nod);
extern void set_default_group_mapping (void);
extern void group_mapping_destroy_all (node nod);
extern uint32_t map_uid_zfs2node (uint32_t uid);
extern uint32_t map_uid_node2zfs (uint32_t uid);
extern uint32_t map_gid_zfs2node (uint32_t gid);
extern uint32_t map_gid_node2zfs (uint32_t gid);
extern void mark_all_users (void);
extern void mark_all_groups (void);
extern void mark_user_mapping (node nod);
extern void mark_group_mapping (node nod);
extern void destroy_marked_users (void);
extern void destroy_marked_groups (void);
extern void destroy_marked_user_mapping (node nod);
extern void destroy_marked_group_mapping (node nod);

extern void initialize_user_group_c (void);
extern void cleanup_user_group_c (void);

#endif
