/* Disjoint interval tree datatype.
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

/* This file contains the datastructure which remembers disjoint intervals.
   When inserting an interval which overlaps with some interval which is
   already in the tree the intervals are merged into one interval.  */

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"
#include "interval.h"
#include "log.h"
#include "memory.h"
#include "splay-tree.h"

/* Create the interval tree, allocate nodes in blocks of PREFERRED_SIZE.  */

interval_tree
interval_tree_create (unsigned preferred_size, pthread_mutex_t *mutex)
{
  interval_tree t;

  t = (interval_tree) xmalloc (sizeof (struct interval_tree_def));
  t->mutex = mutex;
  t->splay = splay_tree_create (preferred_size, NULL, NULL);
  t->preferred_size = preferred_size;
  t->size = 0;
  return t;
}

/* Destroy the interval tree TREE.  */

void
interval_tree_destroy (interval_tree tree)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  splay_tree_destroy (tree->splay);
  free (tree);
}

/* Insert the interval [START, END) into TREE.  */

interval_tree_node
interval_tree_insert (interval_tree tree, uint64_t start, uint64_t end)
{
  splay_tree_node node, prev, next;

  CHECK_MUTEX_LOCKED (tree->mutex);

  if ((node = splay_tree_lookup (tree->splay, start)) != NULL)
    {
      /* The START of interval is already in the tree.  */

      if (INTERVAL_END (node) >= end)
	{
	  /* There already is a larger interval starting in START
	     so we have nothing to do.  */
	  return node;
	}
    }
  else
    {
      /* Lookup the predecessor and successor of key START.  */
      prev = splay_tree_predecessor (tree->splay, start);
      next = splay_tree_successor (tree->splay, start);

      if (prev && INTERVAL_END (prev) >= start)
	{
	  /* We are extending PREV.  */
	  node = prev;
	  if (INTERVAL_END (node) < end)
	    INTERVAL_END (node) = end;
	}
      else if (next && INTERVAL_START (next) <= end)
	{
	  /* We are extending NEXT.  */
	  node = next;
	  if (INTERVAL_START (node) > start)
	    INTERVAL_START (node) = start;
	  if (INTERVAL_END (node) < end)
	    INTERVAL_END (node) = end;
	}
      else
	{
	  /* We are really inserting a new node.  */
	  node = splay_tree_insert (tree->splay, start, end);
	  tree->size++;
	}
    }

  /* Merge the successors if they are covered by [START, END).  */
  while ((next = splay_tree_successor (tree->splay,
				       INTERVAL_START (node))) != NULL)
    {
      if (INTERVAL_START (next) <= INTERVAL_END (node))
	{
	  if (INTERVAL_END (next) > INTERVAL_END (node))
	    INTERVAL_END (node) = INTERVAL_END (next);
	  splay_tree_delete (tree->splay, INTERVAL_START (next));
	  tree->size--;
	}
      else
	break;
    }

  return node;
}

/* Delete the interval [START, END) from TREE.  */

void
interval_tree_delete (interval_tree tree, uint64_t start, uint64_t end)
{
  splay_tree_node node, prev, next;

  CHECK_MUTEX_LOCKED (tree->mutex);

  if ((node = splay_tree_lookup (tree->splay, start)) != NULL)
    {
      if (INTERVAL_END (node) > end)
	{
	  /* We are shortening the interval NODE.  */
	  INTERVAL_START (node) = end;
	  return;
	}
      else
	{
	  splay_tree_delete (tree->splay, start);
	  tree->size--;
	}
    }
  else
    {
      prev = splay_tree_predecessor (tree->splay, start);

      if (prev)
	{
	  if (INTERVAL_END (prev) > end)
	    {
	      /* We are cutting a subinterval from interval PREV.  */
	      splay_tree_insert (tree->splay, end, INTERVAL_END (prev));
	      tree->size++;
	      INTERVAL_END (prev) = start;
	      return;
	    }
	  else
	    {
	      /* We are shortening the interval PREV.  */
	      INTERVAL_END (prev) = start;
	    }
	}
    }

  /* Delete rest intervals which intersect [START, END).  */
  while (1)
    {
      next = splay_tree_successor (tree->splay, start);
      if (INTERVAL_END (next) <= end)
	{
	  splay_tree_delete (tree->splay, INTERVAL_START (next));
	  tree->size--;
	}
      else
	{
	  INTERVAL_START (next) = end;
	  return;
	}
    }
}

/* Return the first interval from tree TREE.  */

interval_tree_node
interval_tree_min (interval_tree tree)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  return splay_tree_min (tree->splay);
}

/* Return the last interval from tree TREE.  */

interval_tree_node
interval_tree_max (interval_tree tree)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  return splay_tree_max (tree->splay);
}

/* Return the interval whose start is lower than KEY.  */

interval_tree_node
interval_tree_predecessor (interval_tree tree, uint64_t key)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  return splay_tree_predecessor (tree->splay, key);
}

/* Return the interval whose start is greater than KEY.  */

interval_tree_node
interval_tree_successor (interval_tree tree, uint64_t key)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  return splay_tree_successor (tree->splay, key);
}

/* Print the interval in NODE to file DATA.  */
static int
print_interval_tree_node (splay_tree_node node, void *data)
{
  FILE *f = (FILE *) data;

  fprintf (f, "[");
  fprintf (f, "%" PRIu64, INTERVAL_START (node));
  fprintf (f, ",");
  fprintf (f, "%" PRIu64, INTERVAL_END (node));
  fprintf (f, ")\n");
  return 0;
}

/* Print the contents of interval tree TREE to file F.  */

void
print_interval_tree (FILE *f, interval_tree tree)
{
  splay_tree_foreach (tree->splay, print_interval_tree_node, f);
}

/* Print the contents of interval tree TREE to STDERR.  */

void
debug_interval_tree (interval_tree tree)
{
  print_interval_tree (stderr, tree);
}
