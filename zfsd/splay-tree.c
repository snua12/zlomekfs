/* A splay-tree datatype.  
   Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

   Some modifications:
   Copyright (C) 2003 Josef Zlomek (josef.zlomek@email.cz).

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

/* For an easily readable description of splay-trees, see:

     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.  */

#include "system.h"
#include <stdlib.h>
#include "alloc-pool.h"
#include "log.h"
#include "memory.h"
#include "splay-tree.h"

static void splay_tree_destroy_helper (splay_tree, splay_tree_node);
static void splay_tree_splay (splay_tree, splay_tree_key);
static splay_tree_node splay_tree_splay_helper (splay_tree, splay_tree_key,
						splay_tree_node *,
						splay_tree_node *,
						splay_tree_node *);
static int splay_tree_foreach_helper (splay_tree, splay_tree_node,
				      splay_tree_foreach_fn, void *);

/* Deallocate NODE (a member of SP), and all its sub-trees.  */

static void 
splay_tree_destroy_helper (splay_tree sp, splay_tree_node node)
{
  if (node->left)
    splay_tree_destroy_helper (sp, node->left);
  if (node->right)
    splay_tree_destroy_helper (sp, node->right);

  if (sp->delete_value)
    (*sp->delete_value)(node->value);

  pool_free (sp->pool, node);
}

/* Help splay SP around KEY.  PARENT and GRANDPARENT are the parent
   and grandparent, respectively, of NODE.  */

static splay_tree_node
splay_tree_splay_helper (splay_tree sp, splay_tree_key key,
			 splay_tree_node *node, splay_tree_node *parent,
			 splay_tree_node *grandparent)
{
  splay_tree_node *next;
  splay_tree_node n;
  
  n = *node;

  if (!n)
    return *parent;

  if (key == n->key)
    /* We've found the target.  */
    next = 0;
  else if (key < n->key)
    /* The target is to the left.  */
    next = &n->left;
  else 
    /* The target is to the right.  */
    next = &n->right;

  if (next)
    {
      /* Continue down the tree.  */
      n = splay_tree_splay_helper (sp, key, next, node, parent);

      /* The recursive call will change the place to which NODE
	 points.  */
      if (*node != n)
	return n;
    }

  if (!parent)
    /* NODE is the root.  We are done.  */
    return n;

  /* First, handle the case where there is no grandparent (i.e.,
     *PARENT is the root of the tree.)  */
  if (!grandparent) 
    {
      if (n == (*parent)->left)
	{
	  *node = n->right;
	  n->right = *parent;
	}
      else
	{
	  *node = n->left;
	  n->left = *parent;
	}
      *parent = n;
      return n;
    }

  /* Next handle the cases where both N and *PARENT are left children,
     or where both are right children.  */
  if (n == (*parent)->left && *parent == (*grandparent)->left)
    {
      splay_tree_node p = *parent;

      (*grandparent)->left = p->right;
      p->right = *grandparent;
      p->left = n->right;
      n->right = p;
      *grandparent = n;
      return n; 
    }
  else if  (n == (*parent)->right && *parent == (*grandparent)->right)
    {
      splay_tree_node p = *parent;

      (*grandparent)->right = p->left;
      p->left = *grandparent;
      p->right = n->left;
      n->left = p;
      *grandparent = n;
      return n;
    }

  /* Finally, deal with the case where N is a left child, but *PARENT
     is a right child, or vice versa.  */
  if (n == (*parent)->left) 
    {
      (*parent)->left = n->right;
      n->right = *parent;
      (*grandparent)->right = n->left;
      n->left = *grandparent;
      *grandparent = n;
      return n;
    } 
  else
    {
      (*parent)->right = n->left;
      n->left = *parent;
      (*grandparent)->left = n->right;
      n->right = *grandparent;
      *grandparent = n;
      return n;
    }
}

/* Splay SP around KEY.  */

static void
splay_tree_splay (splay_tree sp, splay_tree_key key)
{
  if (sp->root == 0)
    return;

  splay_tree_splay_helper (sp, key, &sp->root, 
			   /* grandparent = */ NULL, /* parent = */ NULL); 
}

/* Call FN, passing it the DATA, for every node below NODE, all of
   which are from SP, following an in-order traversal.  If FN every
   returns a non-zero value, the iteration ceases immediately, and the
   value is returned.  Otherwise, this function returns 0.  */

static int
splay_tree_foreach_helper (splay_tree sp, splay_tree_node node,
			   splay_tree_foreach_fn fn, void *data)
{
  int val;

  if (node->left)
    {
      val = splay_tree_foreach_helper (sp, node->left, fn, data);
      if (val)
	return val;
    }

  val = (*fn)(node, data);
  if (val)
    return val;

  if (node->right)
    return splay_tree_foreach_helper (sp, node->right, fn, data);

  return 0;
}

/* Create a new splay tree, using DELETE_VALUE_FN to deallocate
   values.  */

splay_tree 
splay_tree_create (unsigned preferred_size,
		   splay_tree_delete_value_fn delete_value_fn)
{
  splay_tree sp = (splay_tree) xmalloc (sizeof (struct splay_tree_s));
  sp->root = 0;
  sp->delete_value = delete_value_fn;
  sp->pool = create_alloc_pool ("splay", sizeof (struct splay_tree_node_s),
				preferred_size);

  return sp;
}

/* Deallocate SP.  */

void 
splay_tree_destroy (splay_tree sp)
{
  if (sp->root)
    splay_tree_destroy_helper (sp, sp->root);
  free_alloc_pool (sp->pool);
  free (sp);
}

/* Insert a new node (associating KEY with DATA) into SP.  If a
   previous node with the indicated KEY exists, its data is replaced
   with the new value.  Returns the new node.  */

splay_tree_node
splay_tree_insert (splay_tree sp, splay_tree_key key, splay_tree_value value)
{
  splay_tree_splay (sp, key);

  if (sp->root && sp->root->key == key)
    {
      /* If the root of the tree already has the indicated KEY, just
	 replace the value with VALUE.  */
      if (sp->delete_value)
	(*sp->delete_value)(sp->root->value);
      sp->root->value = value;
    } 
  else 
    {
      /* Create a new node, and insert it at the root.  */
      splay_tree_node node;
      
      node = (splay_tree_node) pool_alloc (sp->pool);
      node->key = key;
      node->value = value;
      
      if (!sp->root)
	node->left = node->right = 0;
      else if (sp->root->key < key)
	{
	  node->left = sp->root;
	  node->right = node->left->right;
	  node->left->right = 0;
	}
      else
	{
	  node->right = sp->root;
	  node->left = node->right->left;
	  node->right->left = 0;
	}

      sp->root = node;
    }

  return sp->root;
}

/* Delete KEY from SP.  It is not an error if it did not exist.  */

void
splay_tree_delete (splay_tree sp, splay_tree_key key)
{
  splay_tree_splay (sp, key);

  if (sp->root && sp->root->key == key)
    {
      splay_tree_node left, right;

      left = sp->root->left;
      right = sp->root->right;

      /* Delete the root node itself.  */
      if (sp->delete_value)
	(*sp->delete_value) (sp->root->value);
      pool_free (sp->pool, sp->root);

      /* One of the children is now the root.  Doesn't matter much
	 which, so long as we preserve the properties of the tree.  */
      if (left)
	{
	  sp->root = left;

	  /* If there was a right child as well, hang it off the 
	     right-most leaf of the left child.  */
	  if (right)
	    {
	      while (left->right)
		left = left->right;
	      left->right = right;
	    }
	}
      else
	sp->root = right;
    }
}

/* Lookup KEY in SP, returning VALUE if present, and NULL 
   otherwise.  */

splay_tree_node
splay_tree_lookup (splay_tree sp, splay_tree_key key)
{
  splay_tree_splay (sp, key);

  if (sp->root && sp->root->key == key)
    return sp->root;
  else
    return 0;
}

/* Return the node in SP with the greatest key.  */

splay_tree_node
splay_tree_max (splay_tree sp)
{
  splay_tree_node n = sp->root;

  if (!n)
    return NULL;

  while (n->right)
    n = n->right;

  return n;
}

/* Return the node in SP with the smallest key.  */

splay_tree_node
splay_tree_min (splay_tree sp)
{
  splay_tree_node n = sp->root;

  if (!n)
    return NULL;

  while (n->left)
    n = n->left;

  return n;
}

/* Return the immediate predecessor KEY, or NULL if there is no
   predecessor.  KEY need not be present in the tree.  */

splay_tree_node
splay_tree_predecessor (splay_tree sp, splay_tree_key key)
{
  splay_tree_node node;

  /* If the tree is empty, there is certainly no predecessor.  */
  if (!sp->root)
    return NULL;

  /* Splay the tree around KEY.  That will leave either the KEY
     itself, its predecessor, or its successor at the root.  */
  splay_tree_splay (sp, key);

  /* If the predecessor is at the root, just return it.  */
  if (sp->root->key < key)
    return sp->root;

  /* Otherwise, find the rightmost element of the left subtree.  */
  node = sp->root->left;
  if (node)
    while (node->right)
      node = node->right;

  return node;
}

/* Return the immediate successor KEY, or NULL if there is no
   successor.  KEY need not be present in the tree.  */

splay_tree_node
splay_tree_successor (splay_tree sp, splay_tree_key key)
{
  splay_tree_node node;

  /* If the tree is empty, there is certainly no successor.  */
  if (!sp->root)
    return NULL;

  /* Splay the tree around KEY.  That will leave either the KEY
     itself, its predecessor, or its successor at the root.  */
  splay_tree_splay (sp, key);

  /* If the successor is at the root, just return it.  */
  if (sp->root->key > key)
    return sp->root;

  /* Otherwise, find the leftmost element of the right subtree.  */
  node = sp->root->right;
  if (node)
    while (node->left)
      node = node->left;

  return node;
}

/* Call FN, passing it the DATA, for every node in SP, following an
   in-order traversal.  If FN every returns a non-zero value, the
   iteration ceases immediately, and the value is returned.
   Otherwise, this function returns 0.  */

int
splay_tree_foreach (splay_tree sp, splay_tree_foreach_fn fn, void *data)
{
  if (sp->root)
    return splay_tree_foreach_helper (sp, sp->root, fn, data);

  return 0;
}

/* Print the key and value in NODE to file DATA.  */
static int
print_splay_tree_node (splay_tree_node node, void *data)
{
  FILE *f = (FILE *) data;

  fprintf (f, "[");
  fprintf (f, "%" PRIu64, node->key);
  fprintf (f, "] = ");
  fprintf (f, "%" PRIu64, node->value);
  fprintf (f, "\n");
  return 0;
}

/* Print the contents of interval tree TREE to file F.  */

void
print_splay_tree (FILE *f, splay_tree tree)
{
  splay_tree_foreach (tree, print_splay_tree_node, f);
}

/* Print the contents of interval tree TREE to STDERR.  */

void
debug_splay_tree (splay_tree tree)
{
  print_splay_tree (stderr, tree);
}
