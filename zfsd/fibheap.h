/* A Fibonacci heap datatype.
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Daniel Berlin (dan@cgsoftware.com).

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

/* Fibonacci heaps are somewhat complex, but, there's an article in
   DDJ that explains them pretty well:

   http://www.ddj.com/articles/1997/9701/9701o/9701o.htm?topic=algoritms

   Introduction to algorithms by Corman and Rivest also goes over them.

   The original paper that introduced them is "Fibonacci heaps and their
   uses in improved network optimization algorithms" by Tarjan and
   Fredman (JACM 34(3), July 1987).

   Amortized and real worst case time for operations:

   ExtractMin: O(lg n) amortized. O(n) worst case.
   DecreaseKey: O(1) amortized.  O(lg n) worst case.
   Insert: O(2) amortized. O(1) actual.
   Union: O(1) amortized. O(1) actual.  */

#ifndef FIBHEAP_H
#define FIBHEAP_H

#include "system.h"
#include <inttypes.h>
#include "pthread.h"
#include "alloc-pool.h"

typedef uint32_t fibheapkey_t;
#define FIBHEAPKEY_MIN 0
#define FIBHEAPKEY_MAX ((fibheapkey_t) -1)

typedef struct fibnode_def
{
  struct fibnode_def *parent;
  struct fibnode_def *child;
  struct fibnode_def *left;
  struct fibnode_def *right;
  fibheapkey_t key;
  void *data;
  unsigned int degree : 31;
  unsigned int mark : 1;
} *fibnode;

typedef struct fibheap_def
{
  unsigned int nodes;
  struct fibnode_def *min;
  struct fibnode_def *root;
  pthread_mutex_t *mutex;
  alloc_pool pool;
} *fibheap;

/* The type of a function used to iterate over the tree.
   Returns non-zero value to stop traversing.  */
typedef int (*fibheap_foreach_fn) (void *node_data, void *data);

extern fibheap fibheap_new (unsigned int block_size, pthread_mutex_t *mutex);
extern fibnode fibheap_insert (fibheap, fibheapkey_t, void *);
extern unsigned int fibheap_size (fibheap);
extern fibheapkey_t fibheap_min_key (fibheap);
extern fibnode fibheap_replace_key (fibheap, fibnode, fibheapkey_t);
extern void *fibheap_extract_min (fibheap);
extern void *fibheap_min (fibheap);
extern void *fibheap_delete_node (fibheap, fibnode);
extern void fibheap_delete (fibheap);
extern fibheap fibheap_union (fibheap, fibheap);
extern int fibheap_foreach (fibheap heap, fibheap_foreach_fn fn, void *data);

#endif
