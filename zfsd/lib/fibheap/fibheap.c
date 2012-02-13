/* ! \file \brief A Fibonacci heap datatype.  */

/* Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Daniel Berlin (dan@cgsoftware.com).

   Some modifications for ZFS: Copyright (C) 2003, 2004 Josef Zlomek
   (josef.zlomek@email.cz).

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
#include <stdlib.h>
#include <string.h>
#include "pthread-wrapper.h"
#include "fibheap.h"
#include "memory.h"
#include "log.h"
#include "alloc-pool.h"

static void fibheap_ins_root(fibheap, fibnode);
static void fibheap_rem_root(fibheap, fibnode);
static void fibheap_consolidate(fibheap);
static void fibheap_link(fibheap, fibnode, fibnode);
static void fibheap_cut(fibheap, fibnode, fibnode);
static void fibheap_cascading_cut(fibheap, fibnode);
static fibnode fibheap_extr_min_node(fibheap);
static void fibnode_insert_after(fibnode, fibnode);
#define fibnode_insert_before(a, b) fibnode_insert_after (a->left, b)
static fibnode fibnode_remove(fibnode);
static int fibheap_foreach_helper(fibnode node, fibheap_foreach_fn fn,
								  void *data);


/* ! Create a new fibonacci heap.  */
fibheap fibheap_new(unsigned int block_size, pthread_mutex_t * mutex)
{
	fibheap heap;

	heap = (fibheap) xcalloc(1, sizeof(*heap));
	heap->mutex = mutex;
	heap->pool = create_alloc_pool("fibheap", sizeof(struct fibnode_def),
								   block_size, mutex);
	return heap;
}

/* ! Insert DATA, with priority KEY, into HEAP.  */
fibnode fibheap_insert(fibheap heap, fibheapkey_t key, void *data)
{
	fibnode node;

	CHECK_MUTEX_LOCKED(heap->mutex);

	/* Create the new node.  */
	node = (fibnode) pool_alloc(heap->pool);
	node->parent = NULL;
	node->child = NULL;
	node->left = node;
	node->right = node;
	node->degree = 0;
	node->mark = 0;

	/* Set the node's data.  */
	node->data = data;
	node->key = key;

	/* Insert it into the root list.  */
	fibheap_ins_root(heap, node);

	/* If their was no minimum, or this key is less than the min, it's the new 
	   min.  */
	if (heap->min == NULL || node->key < heap->min->key)
		heap->min = node;

	heap->nodes++;

	return node;
}

/* ! Return the data of the minimum node (if we know it).  */
void *fibheap_min(fibheap heap)
{
	CHECK_MUTEX_LOCKED(heap->mutex);

	/* If there is no min, we can't easily return it.  */
	if (heap->min == NULL)
		return NULL;
	return heap->min->data;
}

/* ! Return the key of the minimum node (if we know it).  */
fibheapkey_t fibheap_min_key(fibheap heap)
{
	CHECK_MUTEX_LOCKED(heap->mutex);

	/* If there is no min, we can't easily return it.  */
	if (heap->min == NULL)
		return FIBHEAPKEY_MAX;
	return heap->min->key;
}

/* ! Union HEAPA and HEAPB into a new heap.  */
fibheap fibheap_union(fibheap heapa, fibheap heapb)
{
	fibnode a_root, b_root, temp;

#ifdef ENABLE_CHECKING
	if (heapa->mutex != heapb->mutex)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(heapa->mutex);

	/* If one of the heaps is empty, the union is just the other heap.  */
	if ((a_root = heapa->root) == NULL)
	{
		free(heapa);
		return heapb;
	}
	if ((b_root = heapb->root) == NULL)
	{
		free(heapb);
		return heapa;
	}

	/* Merge them to the next nodes on the opposite chain.  */
	a_root->left->right = b_root;
	b_root->left->right = a_root;
	temp = a_root->left;
	a_root->left = b_root->left;
	b_root->left = temp;
	heapa->nodes += heapb->nodes;

	/* And set the new minimum, if it's changed.  */
	if (heapb->min->key < heapa->min->key)
		heapa->min = heapb->min;

	free(heapb);
	return heapa;
}

/* ! Extract the data of the minimum node from HEAP.  */
void *fibheap_extract_min(fibheap heap)
{
	fibnode z;
	void *ret = NULL;

	CHECK_MUTEX_LOCKED(heap->mutex);

	/* If we don't have a min set, it means we have no nodes.  */
	if (heap->min != NULL)
	{
		/* Otherwise, extract the min node, free the node, and return the
		   node's data.  */
		z = fibheap_extr_min_node(heap);
		ret = z->data;
		pool_free(heap->pool, z);
	}

	return ret;
}

/* ! Replace the KEY associated with NODE.  */
fibnode fibheap_replace_key(fibheap heap, fibnode node, fibheapkey_t key)
{
	fibnode y;

	CHECK_MUTEX_LOCKED(heap->mutex);

	if (key == node->key)
		return node;

	if (key > node->key)
		return fibheap_insert(heap, key, fibheap_delete_node(heap, node));

	node->key = key;
	y = node->parent;

	/* These two compares are specifically <= 0 to make sure that in the case
	   of equality, a node we replaced the data on, becomes the new min.  This
	   is needed so that delete's call to extractmin gets the right node.  */
	if (y != NULL)
		if (node->key <= y->key)
		{
			fibheap_cut(heap, node, y);
			fibheap_cascading_cut(heap, y);
		}

	if (node->key <= heap->min->key)
		heap->min = node;

	return node;
}

/* ! Delete NODE from HEAP.  */
void *fibheap_delete_node(fibheap heap, fibnode node)
{
	void *ret = node->data;

	CHECK_MUTEX_LOCKED(heap->mutex);

	/* To perform delete, we just make it the min key, and extract.  */
	fibheap_replace_key(heap, node, FIBHEAPKEY_MIN);
	fibheap_extract_min(heap);

	return ret;
}

/* ! Delete HEAP.  */
void fibheap_delete(fibheap heap)
{
	while (heap->min != NULL)
		pool_free(heap->pool, fibheap_extr_min_node(heap));

	free_alloc_pool(heap->pool);
	free(heap);
}

/* ! Return size of the heap HEAP.  */
unsigned int fibheap_size(fibheap heap)
{
	unsigned int n;

	if (heap->mutex)
		zfsd_mutex_lock(heap->mutex);

	n = heap->nodes;

	if (heap->mutex)
		zfsd_mutex_unlock(heap->mutex);

	return n;
}

/* ! Extract the minimum node of the heap.  */
static fibnode fibheap_extr_min_node(fibheap heap)
{
	fibnode ret = heap->min;
	fibnode x, y, orig;

	/* Attach the child list of the minimum node to the root list of the heap.
	   If there is no child list, we don't do squat.  */
	for (x = ret->child, orig = NULL; x != orig && x != NULL; x = y)
	{
		if (orig == NULL)
			orig = x;
		y = x->right;
		x->parent = NULL;
		fibheap_ins_root(heap, x);
	}

	/* Remove the old root.  */
	fibheap_rem_root(heap, ret);
	heap->nodes--;

	/* If we are left with no nodes, then the min is NULL.  */
	if (heap->nodes == 0)
		heap->min = NULL;
	else
	{
		/* Otherwise, consolidate to find new minimum, as well as do the reorg
		   work that needs to be done.  */
		heap->min = ret->right;
		fibheap_consolidate(heap);
	}

	return ret;
}

/* ! Insert NODE into the root list of HEAP.  */
static void fibheap_ins_root(fibheap heap, fibnode node)
{
	/* If the heap is currently empty, the new node becomes the singleton
	   circular root list.  */
	if (heap->root == NULL)
	{
		heap->root = node;
		node->left = node;
		node->right = node;
		return;
	}

	/* Otherwise, insert it in the circular root list between the root and
	   it's right node.  */
	fibnode_insert_after(heap->root, node);
}

/* ! Remove NODE from the rootlist of HEAP.  */
static void fibheap_rem_root(fibheap heap, fibnode node)
{
	if (node->left == node)
		heap->root = NULL;
	else
		heap->root = fibnode_remove(node);
}

/* ! Consolidate the heap.  */
static void fibheap_consolidate(fibheap heap)
{
#define D ((int) (1 + 8 * sizeof (long)))
	fibnode a[D];
	fibnode w;
	fibnode y;
	fibnode x;
	int i;
	int d;

	memset(a, 0, sizeof(fibnode) * D);

	while ((w = heap->root) != NULL)
	{
		x = w;
		fibheap_rem_root(heap, w);
		d = x->degree;
		while (a[d] != NULL)
		{
			y = a[d];
			if (x->key > y->key)
			{
				fibnode temp;
				temp = x;
				x = y;
				y = temp;
			}
			fibheap_link(heap, y, x);
			a[d] = NULL;
			d++;
		}
		a[d] = x;
	}
	heap->min = NULL;
	for (i = 0; i < D; i++)
		if (a[i] != NULL)
		{
			fibheap_ins_root(heap, a[i]);
			if (heap->min == NULL || a[i]->key < heap->min->key)
				heap->min = a[i];
		}
#undef D
}

/* ! Make NODE a child of PARENT.  */
static void
fibheap_link(ATTRIBUTE_UNUSED fibheap heap, fibnode node, fibnode parent)
{
	if (parent->child == NULL)
		parent->child = node;
	else
		fibnode_insert_before(parent->child, node);
	node->parent = parent;
	parent->degree++;
	node->mark = 0;
}

/* ! Remove NODE from PARENT's child list.  */
static void fibheap_cut(fibheap heap, fibnode node, fibnode parent)
{
	fibnode_remove(node);
	parent->degree--;
	fibheap_ins_root(heap, node);
	node->parent = NULL;
	node->mark = 0;
}

static void fibheap_cascading_cut(fibheap heap, fibnode y)
{
	fibnode z;

	while ((z = y->parent) != NULL)
	{
		if (y->mark == 0)
		{
			y->mark = 1;
			return;
		}
		else
		{
			fibheap_cut(heap, y, z);
			y = z;
		}
	}
}

static void fibnode_insert_after(fibnode a, fibnode b)
{
	if (a == a->right)
	{
		a->right = b;
		a->left = b;
		b->right = a;
		b->left = a;
	}
	else
	{
		b->right = a->right;
		a->right->left = b;
		a->right = b;
		b->left = a;
	}
}

static fibnode fibnode_remove(fibnode node)
{
	fibnode ret;

	if (node == node->left)
		ret = NULL;
	else
		ret = node->left;

	if (node->parent != NULL && node->parent->child == node)
		node->parent->child = ret;

	node->right->left = node->left;
	node->left->right = node->right;

	node->parent = NULL;
	node->left = node;
	node->right = node;

	return ret;
}

static int
fibheap_foreach_helper(fibnode node, fibheap_foreach_fn fn, void *data)
{
	fibnode stop_node = node;
	int val;

	do
	{
		val = (*fn) (node->data, data);
		if (val)
			return val;

		if (node->child)
		{
			val = fibheap_foreach_helper(node->child, fn, data);
			if (val)
				return val;
		}

		node = node->right;
	}
	while (node != stop_node);

	return 0;
}

int fibheap_foreach(fibheap heap, fibheap_foreach_fn fn, void *data)
{
	int val = 0;

	CHECK_MUTEX_LOCKED(heap->mutex);

	if (heap->root)
		val = fibheap_foreach_helper(heap->root, fn, data);

	return val;
}
