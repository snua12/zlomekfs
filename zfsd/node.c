/* Node functions.
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
#include <netdb.h>
#include <netinet/in.h>
#include "memory.h"
#include "node.h"

/* Hash table of nodes.  */
static htab_t node_htab;

/* Hash function for node N.  */
#define NODE_HASH(N) ((N)->id)

/* Hash function for node X.  */

static hash_t
node_hash (const void *x)
{
  return NODE_HASH ((node) x);
}

/* Compare a node X with ID *Y.  */

static int
node_eq (const void *x, const void *y)
{
  node n = (node) x;
  unsigned int id = *(unsigned int *) y;

  return n->id == id;
}

/* Create new node with ID and NAME and insert it to hash table.  */

node
node_create (unsigned int id, char *name)
{
  node nod;
  struct hostent *he;
  void **slot;

  nod = (node) xmalloc (sizeof (struct node_def));
  nod->id = id;
  nod->name = xstrdup (name);
  nod->flags = 0;
  nod->status = CONNECTION_NONE;
  nod->clnt = NULL;

  he = gethostbyname (name);
  if (he)
    {
      if (he->h_addrtype == AF_INET
	  && he->h_length == sizeof (nod->addr.sin_addr))
	{
	  nod->flags |= NODE_ADDR_RESOLVED;
	  memcpy (&nod->addr.sin_addr, he->h_addr_list[0], he->h_length);
	}
    }

#ifdef ENABLE_CHECKING
  slot = htab_find_slot (node_htab, &nod->id, NO_INSERT);
  if (slot)
    abort ();
#endif

  slot = htab_find_slot (node_htab, &nod->id, INSERT);
  *slot = nod;

  return nod;
}

/* Initialize data structures in NODE.C.  */

void
initialize_node_c ()
{
  node_htab = htab_create (50, node_hash, node_eq, NULL);
}

/* Destroy data structures in NODE.C.  */

void
cleanup_node_c ()
{
  htab_destroy (node_htab);
}
