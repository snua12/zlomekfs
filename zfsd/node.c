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

/* Create node structure and fill it with information.  */
node
node_create (char *name)
{
  node nod;
  struct hostent *he;

  nod = (node) xmalloc (sizeof (node));
  nod->name = xstrdup (name);
  nod->flags = 0;

  he = gethostbyname (name);
  if (he)
    {
      if (he->h_addrtype == AF_INET && he->h_length == sizeof (nod->addr))
	{
	  nod->flags |= NODE_ADDR_RESOLVED;
	  memcpy (&nod->addr, he->h_addr_list[0], sizeof (nod->addr));
	}
    }

  return nod;
}
