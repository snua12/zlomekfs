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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pthread.h"
#include "fh.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "log.h"
#include "memory.h"
#include "server.h"
#include "varray.h"
#include "zfs_prot.h"

/* File handle of ZFS root.  */
zfs_fh root_fh = {SERVER_ANY, VOLUME_ID_VIRTUAL, VIRTUAL_DEVICE, ROOT_INODE};

/* The virtual directory root.  */
static virtual_dir root;

/* Allocation pool for file handles.  */
static alloc_pool fh_pool;

/* Mutex for fh_pool.  */
pthread_mutex_t fh_pool_mutex;

/* Allocation pool for virtual directories ("mountpoints").  */
static alloc_pool virtual_dir_pool;

/* Hash table of virtual directories, searched by fh.  */
static htab_t virtual_dir_htab;

/* Hash table of virtual directories, searched by (parent->fh, name).  */
static htab_t virtual_dir_htab_name;

/* Mutex for virtual directories.  */
static pthread_mutex_t virtual_dir_mutex;

/* Hash function for virtual_dir VD, computed from fh.  */
#define VIRTUAL_DIR_HASH(VD)						\
  (crc32_buffer (&(VD)->fh, sizeof (zfs_fh)))

/* Hash function for virtual_dir VD, computed from (parent->fh, name).  */
#define VIRTUAL_DIR_HASH_NAME(VD)					\
  (crc32_update (crc32_string ((VD)->name),				\
		 &(VD)->parent->fh, sizeof (zfs_fh)))

/* Hash function for internal file handle X, computed from local_fh.  */

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
  zfs_fh *x = &((internal_fh) xx)->local_fh;
  zfs_fh *y = (zfs_fh *) yy;

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
  internal_fh fh = (internal_fh) x;

  free (fh->name);
  pool_free (fh_pool, x);
}

/* Find the internal file handle or virtual directory for zfs_fh FH
   and set *VOLP, *IFHP and VDP according to it.  */

bool
fh_lookup (zfs_fh *fh, volume *volp, internal_fh *ifhp, virtual_dir *vdp)
{
  hash_t hash = ZFS_FH_HASH (fh);

  if (fh->vid == VOLUME_ID_VIRTUAL)
    {
      virtual_dir vd;

      zfsd_mutex_lock (&virtual_dir_mutex);
      vd = (virtual_dir) htab_find_with_hash (virtual_dir_htab, fh, hash);
      zfsd_mutex_unlock (&virtual_dir_mutex);
      if (!vd)
	return false;

      *volp = vd->vol;
      *ifhp = NULL;
      *vdp = vd;
      return true;
    }
  else
    {
      volume vol;
      internal_fh ifh;

      zfsd_mutex_lock (&volume_mutex);
      vol = volume_lookup (fh->vid);
      if (!vol || !volume_active_p (vol))
	{
	  zfsd_mutex_unlock (&volume_mutex);
	  return false;
	}

      zfsd_mutex_lock (&vol->mutex);
      ifh = (internal_fh) htab_find_with_hash (vol->fh_htab, fh, hash);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&volume_mutex);
      if (!ifh)
	return false;

      *volp = vol;
      *ifhp = ifh;
      *vdp = NULL;
      return true;
    }

  return false;
}

/* Return the virtual directory for NAME in virtual directory PARENT.  */

virtual_dir
vd_lookup_name (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  struct virtual_dir_def tmp_vd;

  tmp_vd.parent = parent;
  tmp_vd.name = (char *) name;
  zfsd_mutex_lock (&virtual_dir_mutex);
  vd = (virtual_dir) htab_find (virtual_dir_htab_name, &tmp_vd);
  zfsd_mutex_unlock (&virtual_dir_mutex);

  return vd;
}

/* Return the internal file handle or virtual directory for NAME in directory
   PARENT on volume VOL.  */

internal_fh
fh_lookup_name (volume vol, internal_fh parent, const char *name)
{
  struct internal_fh_def tmp_fh;
  internal_fh fh;

  tmp_fh.parent = parent;
  tmp_fh.name = (char *) name;

  zfsd_mutex_lock (&vol->mutex);
  fh = (internal_fh) htab_find (vol->fh_htab_name, &tmp_fh);
  zfsd_mutex_unlock (&vol->mutex);
  return fh;
}

/* Create a new internal file handle and store it to hash tables.  */

internal_fh
internal_fh_create (zfs_fh *local_fh, zfs_fh *master_fh, internal_fh parent,
		    volume vol, const char *name, fattr *attr)
{
  internal_fh fh;
  void **slot;

  /* Create a new internal file handle.  */
  zfsd_mutex_lock (&fh_pool_mutex);
  fh = (internal_fh) pool_alloc (fh_pool);
  zfsd_mutex_unlock (&fh_pool_mutex);
  fh->local_fh = *local_fh;
  fh->master_fh = *master_fh;
  fh->parent = parent;
  fh->name = xstrdup (name);
  fh->attr = *attr;

  if (fh->attr.type == FT_DIR)
    varray_create (&fh->dentries, sizeof (internal_fh), 16);
  if (parent)
    {
      fh->dentry_index = VARRAY_USED (parent->dentries);
      VARRAY_PUSH (parent->dentries, fh, internal_fh);
    }

  zfsd_mutex_lock (&vol->mutex);
#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (vol->fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (vol->fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), INSERT);
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

  zfsd_mutex_unlock (&vol->mutex);

  return fh;
}

/* Destroy the internal file handle FH.  */

void
internal_fh_destroy (internal_fh fh, volume vol)
{
  void **slot;
  internal_fh top;

  if (fh->attr.type == FT_DIR)
    {
#ifdef ENABLE_CHECKING
      if (VARRAY_USED (fh->dentries))
	abort ();
#endif
      varray_destroy (&fh->dentries);
    }

  /* Remove FH from parent's directory entries.  */
  if (fh->parent)
    {
      top = VARRAY_TOP (fh->parent->dentries, internal_fh);
      VARRAY_ACCESS (fh->parent->dentries, fh->dentry_index, internal_fh)
	= top;
      VARRAY_POP (fh->parent->dentries);
      top->dentry_index = fh->dentry_index;
    }

  zfsd_mutex_lock (&vol->mutex);
  if (fh->parent)
    {
      slot = htab_find_slot (vol->fh_htab_name, fh, NO_INSERT);
#ifdef ENABLE_CHECKING
      if (!slot)
	abort ();
#endif
      htab_clear_slot (vol->fh_htab_name, slot);
    }

  slot = htab_find_slot_with_hash (vol->fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vol->fh_htab, slot);
  zfsd_mutex_unlock (&vol->mutex);

  free (fh->name);
  zfsd_mutex_lock (&fh_pool_mutex);
  pool_free (fh_pool, fh);
  zfsd_mutex_unlock (&fh_pool_mutex);
}

/* Print the contents of hash table HTAB to file F.  */

void
print_fh_htab (FILE *f, htab_t htab)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (htab, slot,
    {
      internal_fh fh = (internal_fh) *slot;

      fprintf (f, "[%u,%u,%u,%u] ", fh->local_fh.sid, fh->local_fh.vid,
	       fh->local_fh.dev, fh->local_fh.ino);
      fprintf (f, "[%u,%u,%u,%u] ", fh->master_fh.sid, fh->master_fh.vid,
	       fh->master_fh.dev, fh->master_fh.ino);
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
  zfs_fh *x = &((virtual_dir) xx)->fh;
  zfs_fh *y = (zfs_fh *) yy;

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
  virtual_dir_set_fattr (vd);

  varray_create (&vd->subdirs, sizeof (virtual_dir), 16);
  vd->subdir_index = VARRAY_USED (parent->subdirs);
  VARRAY_PUSH (parent->subdirs, vd, virtual_dir);
  vd->parent->attr.nlink++;
  vd->parent->attr.ctime = vd->parent->attr.mtime = time (NULL);

  vd->n_mountpoints = 0;
  vd->vol = NULL;

#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (virtual_dir_htab, &vd->fh,
				   VIRTUAL_DIR_HASH (vd), NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (virtual_dir_htab, &vd->fh,
				   VIRTUAL_DIR_HASH (vd), INSERT);
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

  /* Check the path to root.  */
  zfsd_mutex_lock (&virtual_dir_mutex);
  for (; vd; vd = parent)
    {
      parent = vd->parent;
      vd->n_mountpoints--;
      if (vd->n_mountpoints == 0)
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
	  vd->parent->attr.nlink--;
	  vd->parent->attr.ctime = vd->parent->attr.mtime = time (NULL);

	  /* Delete the virtual_fh from the table of virtual directories.  */
	  slot = htab_find_slot (virtual_dir_htab_name, vd, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (virtual_dir_htab_name, slot);
	  slot = htab_find_slot_with_hash (virtual_dir_htab, &vd->fh,
					   VIRTUAL_DIR_HASH (vd), NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (virtual_dir_htab, slot);
	  free (vd->name);
	}
    }
  zfsd_mutex_unlock (&virtual_dir_mutex);
}

/* Create the virtual root directory.  */

virtual_dir
virtual_root_create ()
{
  virtual_dir root;
  void **slot;

  zfsd_mutex_lock (&virtual_dir_mutex);
  root = (virtual_dir) pool_alloc (virtual_dir_pool);
  root->fh = root_fh;
  root->parent = NULL;
  root->name = xstrdup ("");
  varray_create (&root->subdirs, sizeof (virtual_dir), 16);
  root->subdir_index = 0;
  root->n_mountpoints = 1;
  root->vol = NULL;
  virtual_dir_set_fattr (root);

  /* Insert the root into hash table.  */
  slot = htab_find_slot_with_hash (virtual_dir_htab, &root->fh,
				   VIRTUAL_DIR_HASH (root), INSERT);
  *slot = root;
  zfsd_mutex_unlock (&virtual_dir_mutex);

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

  zfsd_mutex_lock (&virtual_dir_mutex);
  slot = htab_find_slot_with_hash (virtual_dir_htab, &root->fh,
				   VIRTUAL_DIR_HASH (root), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (virtual_dir_htab, slot);
  zfsd_mutex_unlock (&virtual_dir_mutex);
}

/* Create the virtual mountpoint for volume VOL.  */

virtual_dir
virtual_mountpoint_create (volume vol)
{
  varray subpath;
  virtual_dir vd, parent, tmp;
  char *s, *mountpoint;
  unsigned int i;

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
  zfsd_mutex_lock (&virtual_dir_mutex);
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
  varray_destroy (&subpath);
  vd->vol = vol;
  vol->root_vd = vd;

  /* Increase the count of volumes in subtree.  */
  for (tmp = vd; tmp; tmp = tmp->parent)
    tmp->n_mountpoints++;
  zfsd_mutex_unlock (&virtual_dir_mutex);

  free (mountpoint);

  return vd;
}

/* Set the file attributes of virtual directory VD.  */

void
virtual_dir_set_fattr (virtual_dir vd)
{
  vd->attr.type = FT_DIR;
  vd->attr.mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  vd->attr.nlink = 2;
  vd->attr.uid = 0; /* FIXME? */
  vd->attr.gid = 0; /* FIXME? */
  vd->attr.rdev = 0;
  vd->attr.size = 0;
  vd->attr.blocks = 0;
  vd->attr.blksize = 4096;
  vd->attr.generation = 0;
  vd->attr.fversion = 0;
  vd->attr.sid = vd->fh.sid;
  vd->attr.vid = vd->fh.vid;
  vd->attr.dev = vd->fh.dev;
  vd->attr.ino = vd->fh.ino;
  vd->attr.atime = time (NULL);
  vd->attr.mtime = vd->attr.atime;
  vd->attr.ctime = vd->attr.atime;
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
  pthread_mutex_init (&fh_pool_mutex, NULL);
  fh_pool = create_alloc_pool ("fh_pool", sizeof (struct internal_fh_def),
			       1023, &fh_pool_mutex);

  /* Data structures for virtual directories.  */
  pthread_mutex_init (&virtual_dir_mutex, NULL);
  virtual_dir_pool = create_alloc_pool ("virtual_dir_pool",
					sizeof (struct virtual_dir_def), 127,
					&virtual_dir_mutex);
  virtual_dir_htab = htab_create (100, virtual_dir_hash, virtual_dir_eq,
				  virtual_dir_del, &virtual_dir_mutex);
  virtual_dir_htab_name = htab_create (100, virtual_dir_hash_name,
				       virtual_dir_eq_name, NULL,
				       &virtual_dir_mutex);

  root = virtual_root_create ();
}

/* Destroy data structures in FH.C.  */

void
cleanup_fh_c ()
{
  virtual_root_destroy (root);

  /* Data structures for file handles.  */
  zfsd_mutex_lock (&fh_pool_mutex);
#ifdef ENABLE_CHECKING
  if (fh_pool->elts_free < fh_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in fh_pool.\n",
	     fh_pool->elts_allocated - fh_pool->elts_free);
#endif
  free_alloc_pool (fh_pool);
  zfsd_mutex_unlock (&fh_pool_mutex);
  pthread_mutex_destroy (&fh_pool_mutex);

  /* Data structures for virtual directories.  */
  zfsd_mutex_lock (&virtual_dir_mutex);
  htab_destroy (virtual_dir_htab_name);
  htab_destroy (virtual_dir_htab);
#ifdef ENABLE_CHECKING
  if (virtual_dir_pool->elts_free < virtual_dir_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in virtual_dir_pool.\n",
	     virtual_dir_pool->elts_allocated - virtual_dir_pool->elts_free);
#endif
  free_alloc_pool (virtual_dir_pool);
  zfsd_mutex_unlock (&virtual_dir_mutex);
  pthread_mutex_destroy (&virtual_dir_mutex);
}
