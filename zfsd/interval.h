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

#ifndef INTERVAL_H
#define INTERVAL_H

#include "system.h"
#include <inttypes.h>
#include "pthread.h"
#include "log.h"
#include "splay-tree.h"
#include "varray.h"

/* The interval tree.  */
typedef struct interval_tree_def
{
  /* Mutex for this interval tree.  */
  pthread_mutex_t *mutex;

  /* The underlying splay tree.  */
  splay_tree splay;

  /* Preferred size of block for alloc pool.  */
  unsigned int preferred_size;

  /* Number of intervals in tree.  */
  unsigned int size;

  /* File descriptor associated with the tree.  */
  int fd;

  /* Generation of opened file descriptor.  */
  unsigned int generation;
} *interval_tree;

/* Structure of an interval used by interval_tree_read/interval_tree_write.  */
typedef struct interval_def
{
  /* Start of interval.  */
  uint64_t start;

  /* End of interval.  */
  uint64_t end;
} interval;

typedef splay_tree_node interval_tree_node;
#define INTERVAL_START(NODE) ((NODE)->key)
#define INTERVAL_END(NODE) ((NODE)->value)

extern interval_tree interval_tree_create (unsigned int preferred_size,
					   pthread_mutex_t *mutex);
extern void interval_tree_destroy (interval_tree tree);
extern interval_tree_node interval_tree_insert (interval_tree tree,
						uint64_t start, uint64_t end);
extern void interval_tree_delete (interval_tree tree,
				  uint64_t start, uint64_t end);
extern interval_tree_node interval_tree_min (interval_tree tree);
extern interval_tree_node interval_tree_max (interval_tree tree);
extern interval_tree_node interval_tree_predecessor (interval_tree tree,
						     uint64_t key);
extern interval_tree_node interval_tree_successor (interval_tree tree,
						   uint64_t key);
extern bool interval_tree_read (interval_tree tree, int fd, uint64_t n);
extern bool interval_tree_write (interval_tree tree, int fd);
extern void interval_tree_intersection (interval_tree tree, uint64_t start,
					uint64_t end, varray *dest);
extern void interval_tree_intersection_varray (interval_tree tree, varray src,
					       varray *dest);
extern void interval_tree_complement (interval_tree tree, uint64_t start,
				      uint64_t end, varray *dest);
extern void interval_tree_complement_varray (interval_tree tree, varray src,
					     varray *dest);
extern void print_interval_tree (FILE *f, interval_tree tree);
extern void debug_interval_tree (interval_tree tree);

#endif
