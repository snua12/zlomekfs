/* File handle functions.
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
#include <stdio.h>
#include <string.h>
#include "fh.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "server.h"
#include "varray.h"

/* File handle of ZFS root.  */
svc_fh root_fh = {SERVER_ANY, VOLUME_ID_VIRTUAL, VIRTUAL_DEVICE, ROOT_INODE};

/* The virtual directory root.  */
static virtual_dir root;

/* Allocation pool for file handles.  */
alloc_pool fh_pool;

/* Allocation pool for virtual directories ("mountpoints").  */
static alloc_pool virtual_dir_pool;

/* Hash table of virtual directories, searched by fh.  */
static htab_t virtual_dir_htab;

/* Hash table of virtual directories, searched by (parent->fh, name).  */
static htab_t virtual_dir_htab_name;

/* Hash function for virtual_dir VD, computed from fh.  */
#define VIRTUAL_DIR_HASH(VD)						\
  (crc32_buffer (&(VD)->fh, sizeof (svc_fh)))

/* Hash function for virtual_dir VD, computed from (parent->fh, name).  */
#define VIRTUAL_DIR_HASH_NAME(VD)					\
  (crc32_update (crc32_string ((VD)->name),				\
		 &(VD)->parent->fh, sizeof (svc_fh)))

/* Hash function for internal file handle X, computed from client_fh.  */

hash_t
internal_fh_hash (const void *x)
{
  return INTERNAL_FH_HASH ((internal_fh) x);
}

/* Hash function for internal_fh X, computed from parent_fh and name.  */

hash_t
internal_fh_hash_name (const void *x)
{
  return INTERNAL_FH_HASH_NAME ((internal_fh) x);
}

/* Compare an internal file handle XX with client's file handle YY.  */

int
internal_fh_eq (const void *xx, const void *yy)
{
  svc_fh *x = &((internal_fh) xx)->client_fh;
  svc_fh *y = (svc_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare two internal file handles XX and YY whether they have same parent
   and file name.  */

int
internal_fh_eq_name (const void *xx, const void *yy)
{
  internal_fh x = (internal_fh) xx;
  internal_fh y = (internal_fh) yy;

  return (x->parent == y->parent
  	  && strcmp (x->name, y->name) == 0);
}

/* Free the internal file handle X.  */

void
internal_fh_del (void *x)
{
  pool_free (fh_pool, x);
}

/* Find the internal file handle or virtual directory for svc_fh FH
   and set *VOLP, *IFHP and VDP according to it.  */

int
fh_lookup (svc_fh *fh, volume *volp, internal_fh *ifhp, virtual_dir *vdp)
{
  hash_t hash = SVC_FH_HASH (fh);
 
  if (fh->vid == VOLUME_ID_VIRTUAL)
    {
      virtual_dir vd;

      vd = (virtual_dir) htab_find_with_hash (virtual_dir_htab, fh, hash);
      if (!vd || vd->active == 0)
	return 0;

      *volp = vd->vol;
      *ifhp = NULL;
      *vdp = vd;
      return 1;
    }
  else
    {
      volume vol;
      internal_fh ifh;

      vol = volume_lookup (fh->vid);
      if (!vol || !VOLUME_ACTIVE_P (vol))
	return 0;

      ifh = (internal_fh) htab_find_with_hash (vol->fh_htab, fh, hash);
      if (!ifh)
	return 0;

      *volp = vol;
      *ifhp = ifh;
      *vdp = NULL;
      return 1;
    }

  return 0;
}

/* Return the virtual directory for NAME in virtual directory PARENT.  */

virtual_dir 
vd_lookup_name (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  struct virtual_dir_def tmp_vd;

  tmp_vd.parent = parent;
  tmp_vd.name = (char *) name;
  vd = (virtual_dir) htab_find (virtual_dir_htab_name, &tmp_vd);
  if (vd && vd->active)
    return vd;

  return NULL;
}

/* Return the internal file handle or virtual directory for NAME in directory
   PARENT on volume VOL.  */

internal_fh
fh_lookup_name (volume vol, internal_fh parent, const char *name)
{
  struct internal_fh_def tmp_fh;

  tmp_fh.parent = parent;
  tmp_fh.name = (char *) name;

  return (internal_fh) htab_find (vol->fh_htab_name, &tmp_fh);
}

/* Create a new internal file handle and store it to hash tables.  */

internal_fh
internal_fh_create (svc_fh *client_fh, svc_fh *server_fh, internal_fh parent,
		    volume vol, const char *name)
{
  internal_fh fh;
  void **slot;

  /* Create a new internal file handle.  */
  fh = (internal_fh) pool_alloc (fh_pool);
  fh->client_fh = *client_fh;
  fh->server_fh = *server_fh;
  fh->parent = parent;
  fh->name = xstrdup (name);
  fh->fd = -1;

#ifdef ENABLE_CHECKING
  slot = htab_find_slot (vol->fh_htab, fh, NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot (vol->fh_htab, fh, INSERT);
  *slot = fh;

  if (parent)
    {
#ifdef ENABLE_CHECKING
      slot = htab_find_slot (vol->fh_htab_name, fh, NO_INSERT);
      if (slot)
	abort ();
#endif
      slot = htab_find_slot (vol->fh_htab_name, fh, INSERT);
      *slot = fh;
    }

  return fh;
}

/* Destroy the internal file handle FH.  */

void
internal_fh_destroy (internal_fh fh, volume vol)
{
  void **slot;

  if (fh->parent)
    {
      slot = htab_find_slot (vol->fh_htab_name, fh, NO_INSERT);
#ifdef ENABLE_CHECKING
      if (!slot)
	abort ();
#endif
      htab_clear_slot (vol->fh_htab_name, slot);
    }

  slot = htab_find_slot (vol->fh_htab, fh, NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vol->fh_htab, slot);

  pool_free (fh_pool, fh);
}

/* Print the contents of hash table HTAB to file F.  */

void
print_fh_htab (FILE *f, htab_t htab)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (htab, slot,
    {
      internal_fh fh = (internal_fh) *slot;

      fprintf (f, "[%u,%u,%u,%u] ", fh->client_fh.sid, fh->client_fh.vid,
	       fh->client_fh.dev, fh->client_fh.ino);
      fprintf (f, "[%u,%u,%u,%u] ", fh->server_fh.sid, fh->server_fh.vid,
	       fh->server_fh.dev, fh->server_fh.ino);
      fprintf (f, "'%s'", fh->name);
      fprintf (f, "\n");
    });
}

/* Print the contents of hash table of filehandles HTAB to STDERR.  */

void
debug_fh_htab (htab_t htab)
{
  print_fh_htab (stderr, htab);
}

/* Hash function for virtual_dir X, computed from FH.  */

static hash_t
virtual_dir_hash (const void *x)
{
  virtual_dir vd = (virtual_dir) x;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (vd->fh))
    abort ();
#endif

  return VIRTUAL_DIR_HASH (vd);
}

/* Hash function for virtual_dir X, computed from (PARENT->FH, NAME).  */

static hash_t
virtual_dir_hash_name (const void *x)
{
  virtual_dir vd = (virtual_dir) x;

#ifdef ENABLE_CHECKING
  if (!vd->parent || !VIRTUAL_FH_P (vd->parent->fh))
    abort ();
#endif

  return VIRTUAL_DIR_HASH_NAME (vd);
}

/* Compare a virtual directory XX with client's file handle YY.  */

static int
virtual_dir_eq (const void *xx, const void *yy)
{
  svc_fh *x = &((virtual_dir) xx)->fh;
  svc_fh *y = (svc_fh *) yy;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (*x))
    abort ();
  if (!VIRTUAL_FH_P (*y))
    abort ();
#endif
  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare two virtual directories XX and YY whether they have same parent
   and file name.  */

static int
virtual_dir_eq_name (const void *xx, const void *yy)
{
  virtual_dir x = (virtual_dir) xx;
  virtual_dir y = (virtual_dir) yy;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (x->fh))
    abort ();
  if (!y->parent || !VIRTUAL_FH_P (y->parent->fh))
    abort ();
#endif

  return (x->parent == y->parent
	  && strcmp (x->name, y->name) == 0);
}

/* Free the virtual firectory X.  */

void
virtual_dir_del (void *x)
{
  pool_free (virtual_dir_pool, x);
}

/* Create a new virtual directory NAME in virtual directory PARENT.  */

virtual_dir
virtual_dir_create (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  static unsigned int last_virtual_ino;
  void **slot;

  last_virtual_ino++;
  if (last_virtual_ino == 0)
    last_virtual_ino++;

  vd = (virtual_dir) pool_alloc (virtual_dir_pool);
  vd->fh.sid = SERVER_ANY;
  vd->fh.vid = VOLUME_ID_VIRTUAL;
  vd->fh.dev = VIRTUAL_DEVICE;
  vd->fh.ino = last_virtual_ino;
  vd->parent = parent;
  vd->name = xstrdup (name);

  varray_create (&vd->subdirs, sizeof (virtual_dir), 16);
  vd->subdir_index = VARRAY_USED (parent->subdirs);
  VARRAY_PUSH (parent->subdirs, vd, virtual_dir);

  vd->active = 0;
  vd->total = 0;
  vd->vol = NULL;
  
#ifdef ENABLE_CHECKING
  slot = htab_find_slot (virtual_dir_htab, &vd->fh, NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot (virtual_dir_htab, &vd->fh, INSERT);
  *slot = vd;

#ifdef ENABLE_CHECKING
  slot = htab_find_slot (virtual_dir_htab_name, vd, NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot (virtual_dir_htab_name, vd, INSERT);
  *slot = vd;

  return vd;
}

/* Delete a virtual directory VD from all hash tables and free it.  */

void
virtual_dir_destroy (virtual_dir vd)
{
  virtual_dir parent;
  void **slot;
  int was_active;

  was_active = VOLUME_ACTIVE_P (vd->vol);
  
  /* Check the path to root.  */
  for (; vd; vd = parent)
    {
      vd->total--;
      if (was_active)
	vd->active--;

      parent = vd->parent;
      
      if (vd->total == 0)
	{
	  virtual_dir top;

#ifdef ENABLE_CHECKING
	  if (VARRAY_USED (vd->subdirs))
	    abort ();
#endif
	  varray_destroy (&vd->subdirs);

	  /* Remove VD from parent's subdirectories.  */
	  top = VARRAY_TOP (vd->parent->subdirs, virtual_dir);
	  VARRAY_ACCESS (vd->parent->subdirs, vd->subdir_index, virtual_dir)
	    = top;
	  VARRAY_POP (vd->parent->subdirs);
	  top->subdir_index = vd->subdir_index;

	  /* Delete the virtual_fh from the table of virtual directories.  */
	  slot = htab_find_slot (virtual_dir_htab_name, vd, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (virtual_dir_htab_name, slot);
	  slot = htab_find_slot (virtual_dir_htab, &vd->fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (virtual_dir_htab, slot);
	}
    }
}

/* Create the virtual root directory.  */

virtual_dir
virtual_root_create ()
{
  virtual_dir root;
  void **slot;

  root = (virtual_dir) pool_alloc (virtual_dir_pool);

  root->fh = root_fh;
  root->parent = NULL;
  root->name = xstrdup ("");
  varray_create (&root->subdirs, sizeof (virtual_dir), 16);
  root->subdir_index = 0;
  root->active = 1;
  root->total = 1;

  /* Insert the root into hash table.  */
  slot = htab_find_slot (virtual_dir_htab, root, INSERT);
  *slot = root;

  return root;
}

/* Destroy virtual root directory.  */

void
virtual_root_destroy (virtual_dir root)
{
  void **slot;

  free (root->name);

#ifdef ENABLE_CHECKING
  if (VARRAY_USED (root->subdirs))
    abort ();
#endif
  varray_destroy (&root->subdirs);

  slot = htab_find_slot (virtual_dir_htab, root, NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (virtual_dir_htab, slot);
}

/* Create the virtual mountpoint for volume VOL.  */

virtual_dir
virtual_mountpoint_create (volume vol)
{
  varray subpath;
  virtual_dir vd, parent, tmp;
  char *s, *mountpoint;
  unsigned int i;
  int active;

  mountpoint = xstrdup (vol->mountpoint);
  varray_create (&subpath, sizeof (char *), 16);

  /* Split the path.  */
  s = mountpoint;
  while (*s != 0)
    {
      while (*s == '/')
	s++;
      
      if (*s == 0)
	break;

      VARRAY_PUSH (subpath, s, char *);
      while (*s != 0 && *s != '/')
	s++;
      if (*s == '/')
	*s++ = 0;
    }

  /* Create the components of the path.  */
  vd = root;
  for (i = 0; i < VARRAY_USED (subpath); i++)
    {
      struct virtual_dir_def tmp_vd;

      parent = vd;
      s = VARRAY_ACCESS (subpath, i, char *);

      tmp_vd.parent = parent;
      tmp_vd.name = s;
      vd = (virtual_dir) htab_find (virtual_dir_htab_name, &tmp_vd);
      if (!vd)
	vd = virtual_dir_create (parent, s);
#ifdef ENABLE_CHECKING
      if (!VIRTUAL_FH_P (vd->fh))
	abort ();
#endif
    }
  vd->vol = vol;
  vol->root_vd = vd;

  /* Increase the count of volumes in subtree.  */
  active = VOLUME_ACTIVE_P (vol);
  for (tmp = vd; tmp; tmp = tmp->parent)
    {
      if (active)
	tmp->active++;
      tmp->total++;
    }

  free (mountpoint);

  return vd;
}

/* Print the virtual directory VD and its subdirectories to file F
   indented by INDENT spaces.  */

static void
print_virtual_tree_node (FILE *f, virtual_dir vd, unsigned int indent)
{
  unsigned int i;

  for (i = 0; i < indent; i++)
    fputc (' ', f);
    
  fprintf (f, "'%s'", vd->name);
  if (vd->vol)
    fprintf (f, "; VOLUME = '%s'", vd->vol->name);
  fputc ('\n', f);

  for (i = 0; i < VARRAY_USED (vd->subdirs); i++)
    print_virtual_tree_node (f, VARRAY_ACCESS (vd->subdirs, i, virtual_dir),
			     indent + 1);
}

/* Print the virtual tree to file F.  */

void
print_virtual_tree (FILE *f)
{
  print_virtual_tree_node (f, root, 0);
}

/* Print the virtual tree to STDERR.  */

void
debug_virtual_tree ()
{
  print_virtual_tree (stderr);
}

/* Initialize data structures in FH.C.  */

void
initialize_fh_c ()
{
  /* Data structures for file handles.  */
  fh_pool = create_alloc_pool ("fh_pool", sizeof (struct internal_fh_def),
			       1023);

  /* Data structures for virtual directories.  */
  virtual_dir_pool = create_alloc_pool ("virtual_dir_pool",
					sizeof (struct virtual_dir_def), 127);
  virtual_dir_htab = htab_create (100, virtual_dir_hash, virtual_dir_eq,
				  virtual_dir_del);
  virtual_dir_htab_name = htab_create (100, virtual_dir_hash_name,
				       virtual_dir_eq_name, NULL);

  root = virtual_root_create ();
}

/* Destroy data structures in FH.C.  */

void
cleanup_fh_c ()
{
  virtual_root_destroy (root);
  
  /* Data structures for file handles.  */
#ifdef ENABLE_CHECKING
  if (fh_pool->elts_free < fh_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in fh_pool.\n",
	     fh_pool->elts_allocated - fh_pool->elts_free);
#endif
  free_alloc_pool (fh_pool);

  /* Data structures for virtual directories.  */
  htab_destroy (virtual_dir_htab);
#ifdef ENABLE_CHECKING
  if (virtual_dir_pool->elts_free < virtual_dir_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in virtual_dir_pool.\n",
	     virtual_dir_pool->elts_allocated - virtual_dir_pool->elts_free);
#endif
  free_alloc_pool (virtual_dir_pool);
}
