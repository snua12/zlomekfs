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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "interval.h"
#include "log.h"
#include "memory.h"
#include "splay-tree.h"

/* Local function prototypes.  */
static void interval_tree_shrink (interval_tree tree);

/* Create the interval tree, allocate nodes in blocks of PREFERRED_SIZE.  */

interval_tree
interval_tree_create (unsigned preferred_size)
{
  interval_tree t;

  t = xmalloc (sizeof (struct interval_tree_def));
  t->splay = splay_tree_create (preferred_size, NULL);
  t->preferred_size = preferred_size;
  t->size = 0;
  return t;
}

/* Destroy the interval tree TREE.  */

void
interval_tree_destroy (interval_tree tree)
{
  splay_tree_destroy (tree->splay);
  free (tree);
}

/* Insert the interval [START, END) into TREE.  */

void
interval_tree_insert (interval_tree tree, uint64_t start, uint64_t end)
{
  splay_tree_node node, prev, next;

  if ((node = splay_tree_lookup (tree->splay, start)) != NULL)
    {
      /* The START of interval is already in the tree.  */

      if (node->value >= end)
	{
	  /* There already is a larger interval starting in START
	     so we have nothing to do.  */
	  return;
	}
    }
  else
    {
      /* Lookup the predecessor and successor of key START.  */
      prev = splay_tree_predecessor (tree->splay, start);
      next = splay_tree_successor (tree->splay, start);

      if (tree->size == tree->preferred_size
	  && !((prev && prev->value >= start) || (next && next->key <= end)))
	{
	  /* Maybe we will be inserting a new node. Shrink the tree first.  */
	  interval_tree_shrink (tree);

	  /* The nodes may have been merged with others. Lookup them again.  */
	  prev = splay_tree_predecessor (tree->splay, start);
	  next = splay_tree_successor (tree->splay, start);
	}

      if (prev && prev->value >= start)
	{
	  /* We are extending PREV.  */
	  node = prev;
	  if (node->value < end)
	    node->value = end;
	}
      else if (next && next->key <= end)
	{
	  /* We are extending NEXT.  */
	  node = next;
	  if (node->key > start)
	    node->key = start;
	  if (node->value < end)
	    node->value = end;
	}
      else
	{
	  /* We are really inserting a new node.  */
	  node = splay_tree_insert (tree->splay, start, end);
	  tree->size++;
	}
    }

  /* Merge the successors if they are covered by [START, END).  */
  while ((next = splay_tree_successor (tree->splay, node->key)) != NULL)
    {
      if (next->key <= node->value)
	{
	  if (next->value > node->value)
	    node->value = next->value;
	  splay_tree_delete (tree->splay, next->key);
	  tree->size--;
	}
      else
	break;
    }
}

/* Delete the interval [START, END) from TREE.  */

void
interval_tree_delete (interval_tree tree, uint64_t start, uint64_t end)
{
  splay_tree_node node, prev, next;

  if ((node = splay_tree_lookup (tree->splay, start)) != NULL)
    {
      if (node->value > end)
	{
	  /* We are shortening the interval NODE.  */
	  node->key = end;
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
	  if (prev->value > end)
	    {
	      /* We are cutting a subinterval from interval PREV.  */
	      interval_tree_shrink (tree);
	      splay_tree_insert (tree->splay, end, prev->value);
	      tree->size++;
	      prev->value = start;
	      return;
	    }
	  else
	    {
	      /* We are shortening the interval PREV.  */
	      prev->value = end;
	    }
	}
    }

  /* Delete rest intervals which intersect [START, END).  */
  while (1)
    {
      next = splay_tree_successor (tree->splay, start);
      if (next->value <= end)
	{
	  splay_tree_delete (tree->splay, next->key);
	  tree->size--;
	}
      else
	{
	  next->key = end;
	  return;
	}
    }
}

/* Data passed to interval_tree_shrink_foreach.  */
typedef struct interval_tree_shrink_data_def
{
  /* The END of prevous interval.  */
  uint64_t last_end;

  /* The sequential number of curent node.  */
  int index;

  /* The array of nodes.  */
  splay_tree_node *node;

  /* The array "holes" between an interval and its successor interval.  */
  uint64_t *diff;

  /* The indexes of nodes in an array sorted by the size of "holes".  */
  int *pos;
} interval_tree_shrink_data;

/* Fill the interval_tree_shrink_data DATA according to the tree, process
   one NODE.  */

static int
interval_tree_shrink_foreach (splay_tree_node node, void *data)
{
  interval_tree_shrink_data *d = data;

  d->node[d->index] = node;
  d->pos[d->index] = d->index;
  if (d->index)
    d->diff[d->index - 1] = node->key - d->last_end;
  d->last_end = node->value;
  d->index++;
  return 0;
}

/* Find the size of K-th smallest "hole", A and B are the bounds where to
   search for K.  */

static uint64_t
interval_tree_shrink_find_k (interval_tree_shrink_data *x, int k, int a, int b)
{
  int i, j;
  uint64_t m;

  while (a < b)
    {
      i = a;
      j = b;
      m = x->diff[x->pos[(a + b) / 2]];

      do
	{
	  while (x->diff[x->pos[i]] < m)
	    i++;
	  while (x->diff[x->pos[j]] > m)
	    j--;
	  if (i < j)
	    {
	      int sw = x->pos[i];
	      x->pos[i] = x->pos[j];
	      x->pos[j] = sw;
	      i++;
	      j--;
	    }
	}
      while (i < j);

      if (k <= j)
	b = j;
      if (i <= k)
	a = i;
    }

  return x->diff[x->pos[a]];
}

/* If the size of TREE is the maximum size,
   1/3 of the intervals which are close to another interval
   will be merged with the interval they are close to.  */

static void
interval_tree_shrink (interval_tree tree)
{
  int i, n;
  interval_tree_shrink_data d;
  uint64_t threshold;

  if (tree->size < tree->preferred_size)
    return;

  /* Find out the size of "holes" between intervals.  */
  d.last_end = 0;
  d.index = 0;
  d.node = xmalloc (tree->size * sizeof (splay_tree_node));
  d.diff = xmalloc (tree->size * sizeof (uint64_t));
  d.pos = xmalloc (tree->size * sizeof (int));
  splay_tree_foreach (tree->splay, interval_tree_shrink_foreach, &d);

  /* N is the number of nodes that will be deleted.  */
  n = tree->size / 3;
  threshold = interval_tree_shrink_find_k (&d, n - 1, 0, tree->size - 2);

  /* Merge nodes which are closer to each other than THRESHOLD.  */
  i = 0;
  while (n--)
    {
      while (d.diff[i] > threshold)
	{
	  i++;
#ifdef ENABLE_CHECKING
	  if (i + 1 >= tree->size)
	    abort ();
#endif
	}
      d.node[i]->value = d.node[i + 1]->value;
      splay_tree_delete (tree->splay, d.node[i + 1]->key);
      tree->size--;
      d.node[i + 1] = d.node[i];
      i++;
    }
  free (d.node);
  free (d.diff);
  free (d.pos);
}
