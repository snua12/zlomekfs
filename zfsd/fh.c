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
#include <string.h>
#include "fh.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "varray.h"

/* File handle of ZFS root.  */
svc_fh root_fh = {SERVER_ANY, VOLUME_ID_NONE, VIRTUAL_DEVICE, ROOT_INODE};

/* The virtual directory root.  */
virtual_dir root;

/* Last used virtual inode number.  */

/* Allocation pool for file handles.  */
static alloc_pool fh_pool;

/* Allocation pool for virtual directories ("mountpoints").  */
static alloc_pool virtual_dir_pool;

/* Hash table of used file handles, searched by client_fh.  */
static htab_t fh_htab;

/* Hash table of used file handles, searched by (parent_fh, name).  */
static htab_t fh_htab_name;

/* Hash table of virtual directories (mount tree).  */
static htab_t virtual_dir_htab;

/* Hash function for svc_fh FH.  */
#define SVC_FH_HASH(FH) (crc32_buffer ((FH), sizeof (svc_fh)))

/* Hash function for internal_fh FH, computed from client_fh.  */
#define INTERNAL_FH_HASH(FH)						\
  (crc32_buffer (&(FH)->client_fh, sizeof (svc_fh)))

/* Hash function for internal_fh FH, computed from parent_fh and name.  */
#define INTERNAL_FH_HASH_NAME(FH)					\
  (crc32_update (crc32_string ((FH)->name),				\
		 &(FH)->parent->client_fh, sizeof (svc_fh)))

/* Hash function for virtual_dir VD.  */
#define VIRTUAL_DIR_HASH(VD)						\
  (crc32_buffer (&(VD)->virtual_fh, sizeof (svc_fh)))

/* Hash function for internal file handle X, computed from client_fh.  */

static hash_t
internal_fh_hash (const void *x)
{
  return INTERNAL_FH_HASH ((internal_fh) x);
}

/* Hash function for internal_fh X, computed from parent_fh and name.  */
static hash_t
internal_fh_hash_name (const void *x)
{
  return INTERNAL_FH_HASH_NAME ((internal_fh) x);
}

/* Compare an internal file handle XX with client's file handle YY.  */

static int
internal_fh_eq (const void *xx, const void *yy)
{
  svc_fh *x = &((internal_fh) xx)->client_fh;
  svc_fh *y = (svc_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare two internal file handles XX and YY whether they have same parent
   and file name.  */

static int
internal_fh_eq_name (const void *xx, const void *yy)
{
  internal_fh x = (internal_fh) xx;
  internal_fh y = (internal_fh) yy;

  return (x->parent == y->parent
  	  && strcmp (x->name, y->name) == 0);
}

/* Find the internal file handle for svc_fh FH.  */

internal_fh
fh_lookup (svc_fh *fh)
{
  hash_t hash = SVC_FH_HASH (fh);
 
  return (internal_fh) htab_find_with_hash (fh_htab, fh, hash);
}

/* Create a new internal file handle and store it to hash tables.  */

internal_fh
internal_fh_create (svc_fh *client_fh, svc_fh *server_fh, internal_fh parent,
		    const char *name)
{
  internal_fh new_fh;
  internal_fh old_fh;
  void **slot;

  /* Create a new internal file handle.  */
  new_fh = (internal_fh) pool_alloc (fh_pool);
  new_fh->client_fh = *client_fh;
  new_fh->server_fh = *server_fh;
  new_fh->parent = parent;
  new_fh->name = xstrdup (name);
  new_fh->vd = NULL;
  new_fh->fd = -1;

  slot = htab_find_slot (fh_htab, new_fh, INSERT);
  *slot = new_fh;

  slot = htab_find_slot (fh_htab_name, new_fh, NO_INSERT);
  if (slot)
    {
      /* PARENT_FH + NAME is already there so it must be a mountpoint.  */
      old_fh = (internal_fh) *slot;

#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (new_fh->client_fh) == VIRTUAL_FH_P (old_fh->client_fh))
	abort ();
#endif

      if (VIRTUAL_FH_P (old_fh->client_fh))
	{
	  virtual_dir vd;

	  /* Set new underlying file handle for mountpoint.  */
	  vd = (virtual_dir) htab_find (virtual_dir_htab, &old_fh->client_fh);
	  vd->real_fh = new_fh;
	  new_fh->vd = vd;
	}
    }
  else
    {
      /* PARENT_FH + NAME is not there yet so insert it.  */
      slot = htab_find_slot (fh_htab_name, new_fh, INSERT);
      *slot = new_fh;
    }

  return new_fh;
}

/* Destroy the internal file handle FH.  */

void
internal_fh_destroy (internal_fh fh)
{
  void **slot;

  if (VIRTUAL_FH_P (fh->client_fh))
    {
      slot = htab_find_slot (virtual_dir_htab, &fh->client_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
      if (!slot)
	abort ();
#endif
      virtual_dir_destroy ((virtual_dir) *slot);
    }
  else
    {
      /* Find out whether the file handle is the real file handle of some
	 virtual directory.  */
      if (fh->vd)
	{
	  fh->vd->real_fh = NULL;
	}
      else
	{
	  slot = htab_find_slot (fh_htab_name, fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (fh_htab_name, slot);
	}

      slot = htab_find_slot (fh_htab, fh, NO_INSERT);
#ifdef ENABLE_CHECKING
      if (!slot)
	abort ();
#endif
      htab_clear_slot (fh_htab, slot);
      pool_free (fh_pool, fh);
    }
}

/* Hash function for virtual_dir X.  */

static hash_t
virtual_dir_hash (const void *x)
{
#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (((virtual_dir) x)->virtual_fh->client_fh))
    abort ();
#endif
  return VIRTUAL_DIR_HASH ((virtual_dir) x);
}

/* Compare a virtual directory XX with client's file handle YY.  */

static int
virtual_dir_eq (const void *xx, const void *yy)
{
  svc_fh *x = &((virtual_dir) xx)->virtual_fh->client_fh;
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

/* Create a new virtual directory NAME in virtual directory PARENT.  */

virtual_dir
virtual_dir_create (virtual_dir parent, const char *name, volume vol,
		    internal_fh real_fh)
{
  virtual_dir vd;
  internal_fh fh;
  svc_fh sfh;
  static unsigned int last_virtual_ino;
  void **slot;

  last_virtual_ino++;
  if (last_virtual_ino == 0)
    last_virtual_ino++;
  sfh.sid = SERVER_ANY;
  sfh.vid = VOLUME_ID_NONE;
  sfh.dev = VIRTUAL_DEVICE;
  sfh.ino = last_virtual_ino;
  fh = internal_fh_create (&sfh, &sfh, parent->virtual_fh, name);
  vd = (virtual_dir) pool_alloc (virtual_dir_pool);

  fh->vd = vd;
  vd->virtual_fh = fh;
  vd->parent = parent;

  slot = htab_find_slot (fh_htab_name, fh, NO_INSERT);
  if (slot)
    {
      /* Remember and overwrite the original file handle for parent+name.  */
      vd->real_fh = (internal_fh) *slot;
      *slot = fh;
    }
  else
    {
      vd->real_fh = NULL;
    }

  varray_create (&vd->subdirs, sizeof (internal_fh), 16);
  vd->subdir_index = VARRAY_USED (parent->subdirs);
  VARRAY_PUSH (parent->subdirs, fh, internal_fh);

  vd->active = 0;
  vd->total = 0;
  
  vd->vol = vol;
  if (vol)
    {
      virtual_dir tmp;
      int active = VOLUME_ACTIVE_P (vol);

      for (tmp = vd; tmp; tmp = tmp->parent)
	{
	  if (active)
	    tmp->active++;
	  tmp->total++;
	}
    }

#ifdef ENABLE_CHECKING
  slot = htab_find_slot (virtual_dir_htab, vd, NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot (virtual_dir_htab, vd, INSERT);
  *slot = vd;

  return vd;
}

/* Delete a virtual directory VD from all hash tables and free it.  */

void
virtual_dir_destroy (virtual_dir vd)
{
  virtual_dir parent;
  void **slot, **slot2;
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
#ifdef ENABLE_CHECKING
	  if (vd->subdirs.nused)
	    message (2, stderr, "Subdirs remaining in ROOT.\n");
#endif
	  varray_destroy (&vd->subdirs);

	  /* Remove VD from parent's subdirectories.  */
	  VARRAY_ACCESS (vd->parent->subdirs, vd->subdir_index, internal_fh)
	    = VARRAY_TOP (vd->parent->subdirs, internal_fh);
	  VARRAY_POP (vd->parent->subdirs);

	  if (vd->real_fh)
	    {
	      vd->real_fh->vd = NULL;

	      /* Replace the VIRTUAL_FH by REAL_FH in the FH_HTAB_NAME.  */
	      slot = htab_find_slot (fh_htab_name, &vd->virtual_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	      if (!slot)
		abort ();
#endif
	      slot2 = htab_find_slot (fh_htab, &vd->real_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	      if (!slot2)
		abort ();
#endif
	      *slot2 = *slot;
	    }
	  else
	    {
	      slot = htab_find_slot (fh_htab_name, &vd->virtual_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	      if (!slot)
		abort ();
#endif
	      htab_clear_slot (fh_htab_name, slot);
	    }

	  vd->virtual_fh->vd = NULL;
	  /* Delete the virtual_fh from the table of all file handles.  */
	  slot = htab_find_slot (fh_htab, &vd->virtual_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  pool_free (fh_pool, *slot);
	  htab_clear_slot (fh_htab, slot);

	  /* Delete the virtual_fh from the table of virtual directories.  */
	  slot = htab_find_slot (virtual_dir_htab, &vd->virtual_fh, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (virtual_dir_htab, slot);

#ifdef ENABLE_CHECKING
	  /* FIXME: delete the subdir from parent directory.  */
	  if (VARRAY_USED (vd->subdirs))
	    abort ();
#endif
	  varray_destroy (&vd->subdirs);
	  pool_free (virtual_dir_pool, vd);
	}
    }
}

virtual_dir
virtual_root_create ()
{
  virtual_dir root;
  internal_fh fh;

  fh = (internal_fh) pool_alloc (fh_pool);
  root = (virtual_dir) pool_alloc (virtual_dir_pool);

  fh->client_fh = root_fh;
  fh->server_fh = root_fh;
  fh->parent = NULL;
  fh->vd = root;
  fh->name = xstrdup ("");
  fh->fd = -1;
  
  root->virtual_fh = fh;
  root->parent = NULL;
  varray_create (&root->subdirs, sizeof (internal_fh), 16);
  root->subdir_index = 0;
  root->active = 1;
  root->total = 1;
#if 0
  if (vol)
    {
      /* vyplni se az se bude pristupovat na volume */
      root->virtual_fh->server_fh = file handle na volume;


      root->real_fh = file handle na lokalni disk;
      root->vol = vol;
    }
#else
  root->real_fh = NULL;
  root->vol = NULL;
#endif

  return root;
}

void
virtual_root_destroy (virtual_dir root)
{
  free (root->virtual_fh->name);
#ifdef ENABLE_CHECKING
  if (root->subdirs.nused)
    message (2, stderr, "Subdirs remaining in ROOT.\n");
#endif
  varray_destroy (&root->subdirs);
  pool_free (fh_pool, root->virtual_fh);
  pool_free (virtual_dir_pool, root);
}

/* Initialize data structures in FH.C.  */

void
initialize_fh_c ()
{
  /* Data structures for file handles.  */
  fh_pool = create_alloc_pool ("fh_pool", sizeof (struct internal_fh_def),
			       1023);
  fh_htab = htab_create (1000, internal_fh_hash, internal_fh_eq, NULL);
  fh_htab_name = htab_create (1000, internal_fh_hash_name, internal_fh_eq_name,
			      NULL);

  /* Data structures for virtual directories.  */
  virtual_dir_pool = create_alloc_pool ("virtual_dir_pool",
					sizeof (struct virtual_dir_def), 127);
  virtual_dir_htab = htab_create (100, virtual_dir_hash, virtual_dir_eq, NULL);

  root = virtual_root_create ();
}

/* Destroy data structures in FH.C.  */

void
cleanup_fh_c ()
{
  virtual_root_destroy (root);
  
  /* Data structures for file handles.  */
  htab_destroy (fh_htab);
  htab_destroy (fh_htab_name);
#ifdef ENABLE_CHECKING
  if (fh_pool->elts_free < fh_pool->elts_allocated)
    message (2, stderr, "Memory leak in fh_pool.\n");
#endif
  free_alloc_pool (fh_pool);

  /* Data structures for virtual directories.  */
  htab_destroy (virtual_dir_htab);
#ifdef ENABLE_CHECKING
  if (virtual_dir_pool->elts_free < virtual_dir_pool->elts_allocated)
    message (2, stderr, "Memory leak in virtual_dir_pool.\n");
#endif
  free_alloc_pool (virtual_dir_pool);
}
