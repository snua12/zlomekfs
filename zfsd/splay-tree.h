/*! A splay-tree datatype.
   Copyright 1998, 1999, 2000, 2002 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

   Some modifications for ZFS:
   Copyright (C) 2003, 2004 Josef Zlomek (josef.zlomek@email.cz).

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

/*! For an easily readable description of splay-trees, see:

     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.

   The major feature of splay trees is that all basic tree operations
   are amortized O(log n) time for a tree with n nodes.  */

#ifndef SPLAY_TREE_H
#define SPLAY_TREE_H

#include "system.h"
#include <stdio.h>
#include <inttypes.h>
#include "pthread.h"
#include "alloc-pool.h"

/*! Use typedefs for the key and data types to facilitate changing
   these types, if necessary.  These types should be sufficiently wide
   that any pointer or scalar can be cast to these types, and then
   cast back, without loss of precision.  */
typedef uint64_t splay_tree_key;
typedef uint64_t splay_tree_value;

/*! Forward declaration for a node in the tree.  */
typedef struct splay_tree_node_s *splay_tree_node;

/*! The type of a function used to deallocate any resources associated
   with the value.  */
typedef void (*splay_tree_delete_value_fn) (splay_tree_value);

/*! The type of a function used to iterate over the tree.  */
typedef int (*splay_tree_foreach_fn) (splay_tree_node, void*);

/*! The type of a function used to allocate memory for tree root and
   node structures.  The first argument is the number of bytes needed;
   the second is a data pointer the splay tree functions pass through
   to the allocator.  This function must never return zero.  */
typedef void * (*splay_tree_allocate_fn) (int, void *);

/*! The type of a function used to free memory allocated using the
   corresponding splay_tree_allocate_fn.  The first argument is the
   memory to be freed; the latter is a data pointer the splay tree
   functions pass through to the freer.  */
typedef void (*splay_tree_deallocate_fn) (void *, void *);

/*! The nodes in the splay tree.  */
struct splay_tree_node_s
{
  /* The key.  */
  splay_tree_key key;

  /* The value.  */
  splay_tree_value value;

  /* The left and right children, respectively.  */
  splay_tree_node left;
  splay_tree_node right;
};

/*! The splay tree itself.  */
struct splay_tree_s
{
  /* Mutex for this splay tree.  */
  pthread_mutex_t *mutex;

  /* The root of the tree.  */
  splay_tree_node root;

  /* The deallocate-value function.  NULL if no cleanup is necessary.  */
  splay_tree_delete_value_fn delete_value;

  /* Alloc pool for splay_tree_node.  */
  alloc_pool pool;
};
typedef struct splay_tree_s *splay_tree;

extern splay_tree splay_tree_create (unsigned, splay_tree_delete_value_fn,
				     pthread_mutex_t *mutex);
extern void splay_tree_destroy (splay_tree);
extern void splay_tree_empty (splay_tree);
extern splay_tree_node splay_tree_insert (splay_tree, splay_tree_key,
					  splay_tree_value);
extern void splay_tree_delete (splay_tree, splay_tree_key);
extern splay_tree_node splay_tree_lookup (splay_tree, splay_tree_key);
extern splay_tree_node splay_tree_predecessor (splay_tree, splay_tree_key);
extern splay_tree_node splay_tree_successor (splay_tree, splay_tree_key);
extern splay_tree_node splay_tree_max (splay_tree);
extern splay_tree_node splay_tree_min (splay_tree);
extern int splay_tree_foreach (splay_tree, splay_tree_foreach_fn, void *);
extern void print_splay_tree (FILE *f, splay_tree tree);
extern void debug_splay_tree (splay_tree tree);

#endif /* _SPLAY_TREE_H */
