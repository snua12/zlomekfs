/* Disjoint interval tree datatype.
   Copyright (C) 2003, 2004 Josef Zlomek

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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"
#include "interval.h"
#include "log.h"
#include "memory.h"
#include "splay-tree.h"
#include "util.h"
#include "data-coding.h"
#include "varray.h"

/* Number of intervals being read/written using 1 syscall.  */
#define INTERVAL_COUNT 1024

/* Return the maximum of X and Y.  */
#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Return the minimum of X and Y.  */
#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Create the interval tree, allocate nodes in blocks of PREFERRED_SIZE.  */

interval_tree
interval_tree_create (unsigned int preferred_size, pthread_mutex_t *mutex)
{
  interval_tree t;

  t = (interval_tree) xmalloc (sizeof (struct interval_tree_def));
  t->mutex = mutex;
  t->splay = splay_tree_create (preferred_size, NULL, NULL);
  t->preferred_size = preferred_size;
  t->size = 0;
  t->fd = -1;
  t->generation = 0;
  t->deleted = false;
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
      tree->deleted = true;
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

      if (prev && start < INTERVAL_END (prev))
	{
	  tree->deleted = true;
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
      if (!next || INTERVAL_START (next) >= end)
	break;

      tree->deleted = true;
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

/* Return true if interval [START, END) is covered by the tree TREE.  */

bool
interval_tree_covered (interval_tree tree, uint64_t start, uint64_t end)
{
  interval_tree_node node;

  node = splay_tree_lookup (tree->splay, start);
  if (!node)
    {
      node = splay_tree_predecessor (tree->splay, start);
      if (!node)
	return false;
    }

  return (end <= INTERVAL_END (node));
}

/* Read N intervals of interval tree TREE from file descriptor FD.
   Position in FD should be at the beginning.  */

bool
interval_tree_read (interval_tree tree, int fd, uint64_t n)
{
  interval intervals[INTERVAL_COUNT];
  int i, block;
  bool r;

  CHECK_MUTEX_LOCKED (tree->mutex);

  for (; n > 0; n -= block)
    {
      block = n > INTERVAL_COUNT ? INTERVAL_COUNT : n;

      r = full_read (fd, intervals, block * sizeof (interval));
      if (!r)
	return false;

      for (i = 0; i < block; i++)
	{
	  intervals[i].start = le_to_u64 (intervals[i].start);
	  intervals[i].end = le_to_u64 (intervals[i].end);
	  interval_tree_insert (tree, intervals[i].start, intervals[i].end);
	}
    }

  return true;
}

/* Data used by interval_tree_write_*.  */
typedef struct interval_tree_write_data_def
{
  /* Number of intervals.  */
  int n;

  /* Intervals.  */
  interval intervals[INTERVAL_COUNT];
} interval_tree_write_data;

/* Add interval contained in NODE to buffer DATA and if the buffer is full
   write if to file descriptor FD.  */

static bool
interval_tree_write_1 (interval_tree_node node, int fd,
		       interval_tree_write_data *data)
{
  bool r;

  /* Process left subtree.  */
  if (node->left)
    {
      r = interval_tree_write_1 (node->left, fd, data);
      if (!r)
	return r;
    }

  /* Process current node.  */
  data->intervals[data->n].start = u64_to_le (INTERVAL_START (node));
  data->intervals[data->n].end = u64_to_le (INTERVAL_END (node));
  data->n++;
  if (data->n == INTERVAL_COUNT)
    {
      r = full_write (fd, data->intervals, INTERVAL_COUNT * sizeof (interval));
      if (!r)
	return r;
      data->n = 0;
    }

  /* Process right subtree.  */
  if (node->right)
    {
      r = interval_tree_write_1 (node->right, fd, data);
      if (!r)
	return r;
    }

  return true;
}

/* Write the contents of interval tree TREE to file descriptor FD.
   Position in FD should be at the beginning and FD should be truncated.  */

bool
interval_tree_write (interval_tree tree, int fd)
{
  interval_tree_write_data data;
  bool r;

  CHECK_MUTEX_LOCKED (tree->mutex);

  if (!tree->splay->root)
    {
      /* The tree is empty.  */
      return true;
    }

  data.n = 0;
  r = interval_tree_write_1 (tree->splay->root, fd, &data);
  if (!r)
    return r;

  if (data.n > 0)
    {
      r = full_write (fd, data.intervals, data.n * sizeof (interval));
    }

  return r;
}

/* Add the intersections of interval [START, END) with interval tree TREE
   to varray DEST.  */

static void
interval_tree_intersection_1 (interval_tree tree, uint64_t start, uint64_t end,
			      varray *dest)
{
  interval_tree_node node;

  node = splay_tree_lookup (tree->splay, start);
  if (!node)
    {
      node = splay_tree_predecessor (tree->splay, start);
      if (!node || INTERVAL_END (node) <= start)
	node = splay_tree_successor (tree->splay, start);
    }
  if (!node)
    node = splay_tree_successor (tree->splay, start);

  for (; node && INTERVAL_START (node) < end;
       node = splay_tree_successor (tree->splay, INTERVAL_START (node)))
    {
      interval *x;

      VARRAY_EMPTY_PUSH (*dest);
      x = &VARRAY_TOP (*dest, interval);
      x->start = MAX (start, INTERVAL_START (node));
      x->end = MIN (end, INTERVAL_END (node));
    }
}

/* Store the intersection of interval [START, END) with interval tree TREE
   to varray DEST.  */

void
interval_tree_intersection (interval_tree tree, uint64_t start, uint64_t end,
			    varray *dest)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  varray_create (dest, sizeof (interval), 4);
  interval_tree_intersection_1 (tree, start, end, dest);
}

/* Store the intersection of intervals in varray SRC with interval tree TREE
   to varray DEST.  */

void
interval_tree_intersection_varray (interval_tree tree, varray *src,
				   varray *dest)
{
  unsigned int i;

  CHECK_MUTEX_LOCKED (tree->mutex);

  varray_create (dest, sizeof (interval), 16);
  for (i = 0; i < VARRAY_USED (*src); i++)
    {
      interval *x;

      x = &VARRAY_ACCESS (*src, i, interval);
      interval_tree_intersection_1 (tree, x->start, x->end, dest);
    }
}

/* Add the parts of interval [START, END) which are not in TREE
   to varray DEST.  */

static void
interval_tree_complement_1 (interval_tree tree, uint64_t start, uint64_t end,
			    varray *dest)
{
  interval_tree_node node;
  uint64_t last;

  node = splay_tree_lookup (tree->splay, start);
  if (node)
    {
      last = INTERVAL_END (node);
    }
  else
    {
      node = splay_tree_predecessor (tree->splay, start);
      if (node && INTERVAL_END (node) > start)
	last = INTERVAL_END (node);
      else
	last = start;
    }
  node = splay_tree_successor (tree->splay, start);

  while (last < end)
    {
      interval *x;

      VARRAY_EMPTY_PUSH (*dest);
      x = &VARRAY_TOP (*dest, interval);
      x->start = last;

      if (node)
	{
	  x->end = INTERVAL_START (node);
	  last = INTERVAL_END (node);
	  node = splay_tree_successor (tree->splay, INTERVAL_START (node));
	}
      else
	{
	  x->end = end;
	  break;
	}
    }
}

/* Store the parts of interval [START, END) which are not in TREE
   to varray DEST.  */

void
interval_tree_complement (interval_tree tree, uint64_t start, uint64_t end,
			  varray *dest)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  varray_create (dest, sizeof (interval), 4);
  interval_tree_complement_1 (tree, start, end, dest);
}

/* Store the intersection of intervals in varray SRC with interval tree TREE
   to varray DEST.  */

void
interval_tree_complement_varray (interval_tree tree, varray *src, varray *dest)
{
  unsigned int i;

  CHECK_MUTEX_LOCKED (tree->mutex);

  varray_create (dest, sizeof (interval), 16);
  for (i = 0; i < VARRAY_USED (*src); i++)
    {
      interval *x;

      x = &VARRAY_ACCESS (*src, i, interval);
      interval_tree_complement_1 (tree, x->start, x->end, dest);
    }
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
