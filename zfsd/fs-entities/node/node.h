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

#ifndef NODE_H
#define NODE_H

#include "system.h"
#include <netdb.h>
#include "memory.h"
#include "pthread-wrapper.h"
#include "hashtab.h"

/* ! \brief Node description.  */
typedef struct node_def
{
#ifdef ENABLE_CHECKING
	long unused0;
	long unused1;
#endif

	pthread_mutex_t mutex;
	uint32_t id;				/* !< ID of the node */
	string name;				/* !< name of the node */
	string host_name;			/* !< DNS name or IP address of the node */
	uint16_t port;				/* !< node TCP port */
	time_t last_connect;		/* !< last attemp to connect to node */
	int fd;						/* !< file descriptor */
	unsigned int generation;	/* !< generation of open file descriptor */
	bool marked;				/* !< Is the node marked? */

	/* Tables for mapping between ZFS IDs and node IDs.  */
	htab_t map_uid_to_node;
	htab_t map_uid_to_zfs;
	htab_t map_gid_to_node;
	htab_t map_gid_to_zfs;
} *node;

/* ! Predefined node IDs.  */
#define NODE_ID_NONE 0			/* ID for non-existing node, used as SID in
								   file handle of virtual directory.  */

/* ! Mutex for table of nodes.  */
extern pthread_mutex_t node_mutex;

/* ! Description of local node.  */
extern node this_node;

extern bool is_valid_node_id(uint32_t id);
extern bool is_valid_node_name(const char * name);
extern bool is_valid_host_name(const char * name);

/* ! Function prototypes.  */
extern hash_t node_hash_name(const void *x);
extern int node_eq_name(const void *x, const void *y);
extern node node_lookup(uint32_t id);
extern node node_lookup_name(string * name);
extern node node_create(uint32_t id, string * name, string * host_name, uint16_t tcp_port);
extern node try_create_node(uint32_t id, string * name, string * host_name, uint16_t tcp_port);
extern void mark_all_nodes(void);
extern void destroy_marked_nodes(void);
extern void initialize_node_c(void);
extern void cleanup_node_c(void);
extern void init_this_node(void);

#endif
