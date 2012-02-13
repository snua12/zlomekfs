/* ! \file \brief Node functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "pthread-wrapper.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "user-group.h"
#include "zfs_config.h"

/* ! Hash table of nodes, searched by ID.  */
static htab_t node_htab_sid;

/* ! Hash table of nodes, searched by NAME.  */
static htab_t node_htab_name;

/* ! Mutex for table of nodes.  */
pthread_mutex_t node_mutex;

/* ! Description of local node.  */
node this_node;

bool is_valid_node_id(uint32_t id)
{
	return (id != 0) && (id != ((uint32_t) - 1));
}

bool is_valid_node_name(const char * name)
{
	return (name != NULL) && (name[0] != 0);
}

bool is_valid_host_name(const char * name)
{
	return (name != NULL) && (name[0] != 0);
}

/* ! Hash function for node ID.  */
#define HASH_NODE_ID(ID) (ID)

/* ! Hash function for node NODE, computed from ID.  */
#define NODE_HASH(NODE) ((NODE)->id)

/* ! Hash function for node name.  */
#define HASH_NODE_NAME(NAME) crc32_buffer ((NAME).str, (NAME).len)

/* ! Hash function for node NODE, computed from its name.  */
#define NODE_HASH_NAME(NODE) HASH_NODE_NAME ((NODE)->name)

/* ! Hash function for node X, computed from ID.  */

static hash_t node_hash(const void *x)
{
	return NODE_HASH((const struct node_def *)x);
}

/* ! Hash function for node X, computed from node name.  */

hash_t node_hash_name(const void *x)
{
	return NODE_HASH_NAME((const struct node_def *)x);
}

/* ! Compare a node X with ID *Y.  */

static int node_eq(const void *x, const void *y)
{
	const struct node_def *nod = (const struct node_def *)x;
	uint32_t id = *(const uint32_t *)y;

	return nod->id == id;
}

/* ! Compare a name of node X with string Y.  */

int node_eq_name(const void *x, const void *y)
{
	const struct node_def *nod = (const struct node_def *)x;
	const string *s = (const string *)y;

	return (nod->name.len == s->len && strcmp(nod->name.str, s->str) == 0);
}

/* ! Return the node with id ID.  */

node node_lookup(uint32_t id)
{
	node nod;

	zfsd_mutex_lock(&node_mutex);
	nod = (node) htab_find_with_hash(node_htab_sid, &id, HASH_NODE_ID(id));
	if (nod)
		zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);

	return nod;
}

/* ! Return the node whose name is NAME.  */

node node_lookup_name(string * name)
{
	node nod;

	zfsd_mutex_lock(&node_mutex);
	nod = (node) htab_find_with_hash(node_htab_name, name,
									 HASH_NODE_NAME(*name));
	if (nod)
		zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);

	return nod;
}

/* ! Create new node with ID and NAME and insert it to hash table.  */

node node_create(uint32_t id, string * name, string * host_name, uint16_t tcp_port)
{
	node nod;
	void **slot;

	CHECK_MUTEX_LOCKED(&node_mutex);

	nod = (node) xmalloc(sizeof(struct node_def));
	nod->id = id;
	xstringdup(&nod->name, name);
	xstringdup(&nod->host_name, host_name);
	nod->port = tcp_port;
	nod->last_connect = 0;
	nod->fd = -1;
	nod->generation = 0;
	nod->marked = false;
	nod->map_uid_to_node = NULL;
	nod->map_uid_to_zfs = NULL;
	nod->map_gid_to_node = NULL;
	nod->map_gid_to_zfs = NULL;

	/* Are we creating a structure describing local node? */
	if (strcmp(name->str, zfs_config.this_node.node_name.str) == 0)
	{
		this_node = nod;
		nod->map_uid_to_node = htab_create(5, map_id_to_node_hash,
										   map_id_to_node_eq, NULL,
										   &nod->mutex);
		nod->map_uid_to_zfs = htab_create(5, map_id_to_zfs_hash,
										  map_id_to_zfs_eq, NULL, &nod->mutex);
		nod->map_gid_to_node = htab_create(5, map_id_to_node_hash,
										   map_id_to_node_eq, NULL,
										   &nod->mutex);
		nod->map_gid_to_zfs = htab_create(5, map_id_to_zfs_hash,
										  map_id_to_zfs_eq, NULL, &nod->mutex);
	}

	zfsd_mutex_init(&nod->mutex);
	zfsd_mutex_lock(&nod->mutex);

	slot = htab_find_slot_with_hash(node_htab_sid, &nod->id, NODE_HASH(nod),
									INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = nod;

	slot = htab_find_slot_with_hash(node_htab_name, &nod->name,
									NODE_HASH_NAME(nod), INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = nod;

	return nod;
}

node try_create_node(uint32_t id, string * name, string * host_name, uint16_t tcp_port)
{
	void **slot, **slot2;
	node nod;

	zfsd_mutex_lock(&node_mutex);
	slot = htab_find_slot_with_hash(node_htab_sid, &id, HASH_NODE_ID(id),
									NO_INSERT);
	slot2 = htab_find_slot_with_hash(node_htab_name, name,
									 HASH_NODE_NAME(*name), NO_INSERT);
	if (slot && slot2 && *slot == *slot2)
	{
		nod = (node) * slot;

		zfsd_mutex_lock(&nod->mutex);
		nod->marked = false;
		zfsd_mutex_unlock(&nod->mutex);
		zfsd_mutex_unlock(&node_mutex);
		return NULL;
	}
	if (slot || slot2)
	{
		if (slot)
			message(LOG_NOTICE, FACILITY_DATA | FACILITY_NET,
					"Node with ID = %" PRIu32 " already exists\n", id);
		if (slot2)
			message(LOG_NOTICE, FACILITY_DATA | FACILITY_NET,
					"Node with name = %s already exists\n", name->str);
		zfsd_mutex_unlock(&node_mutex);
		return NULL;
	}

	nod = node_create(id, name, host_name, tcp_port);
	zfsd_mutex_unlock(&node_mutex);
	return nod;
}

/* ! Destroy node NOD and free memory associated with it. This function
   expects node_mutex to be locked.  */

static void node_destroy(node nod)
{
	void **slot;

	CHECK_MUTEX_LOCKED(&node_mutex);
	CHECK_MUTEX_LOCKED(&nod->mutex);

	slot = htab_find_slot_with_hash(node_htab_sid, &nod->id, NODE_HASH(nod),
									NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif
	htab_clear_slot(node_htab_sid, slot);

	slot = htab_find_slot_with_hash(node_htab_name, &nod->name,
									NODE_HASH_NAME(nod), NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif
	htab_clear_slot(node_htab_name, slot);

	if (nod->map_uid_to_node)
	{
		user_mapping_destroy_all(nod);
		htab_destroy(nod->map_uid_to_node);
		htab_destroy(nod->map_uid_to_zfs);

		group_mapping_destroy_all(nod);
		htab_destroy(nod->map_gid_to_node);
		htab_destroy(nod->map_gid_to_zfs);
	}

	zfsd_mutex_unlock(&nod->mutex);
	zfsd_mutex_destroy(&nod->mutex);
	free(nod->host_name.str);
	free(nod->name.str);
	free(nod);
}

/* ! Mark all nodes.  */

void mark_all_nodes(void)
{
	void **slot;

	zfsd_mutex_lock(&node_mutex);
	HTAB_FOR_EACH_SLOT(node_htab_sid, slot)
	{
		node nod = (node) * slot;

		zfsd_mutex_lock(&nod->mutex);
		nod->marked = true;
		zfsd_mutex_unlock(&nod->mutex);
	}
	zfsd_mutex_unlock(&node_mutex);
}

/* ! Destroy marked nodes.  */

void destroy_marked_nodes(void)
{
	void **slot;

	zfsd_mutex_lock(&node_mutex);
	HTAB_FOR_EACH_SLOT(node_htab_sid, slot)
	{
		node nod = (node) * slot;

		zfsd_mutex_lock(&nod->mutex);
		if (nod->marked)
			node_destroy(nod);
		else
			zfsd_mutex_unlock(&nod->mutex);
	}
	zfsd_mutex_unlock(&node_mutex);
}

/* ! Initialize local node so that we could read configuration.  */
void init_this_node(void)
{
	node nod;

	zfsd_mutex_lock(&node_mutex);
	nod = node_create(zfs_config.this_node.node_id,
			&zfs_config.this_node.node_name,
			&zfs_config.this_node.node_name, //TODO: use host_name instead of node_name
			zfs_config.this_node.host_port);

	zfsd_mutex_unlock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
}

/* ! Initialize data structures in NODE.C.  */

void initialize_node_c(void)
{
	zfsd_mutex_init(&node_mutex);
	node_htab_sid = htab_create(50, node_hash, node_eq, NULL, &node_mutex);
	node_htab_name = htab_create(50, node_hash_name, node_eq_name, NULL,
								 &node_mutex);
}

/* ! Destroy data structures in NODE.C.  */

void cleanup_node_c(void)
{
	void **slot;

	zfsd_mutex_lock(&node_mutex);
	HTAB_FOR_EACH_SLOT(node_htab_sid, slot)
	{
		node nod = (node) * slot;

		zfsd_mutex_lock(&nod->mutex);
		node_destroy(nod);
	}
	htab_destroy(node_htab_sid);
	htab_destroy(node_htab_name);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_destroy(&node_mutex);
}
