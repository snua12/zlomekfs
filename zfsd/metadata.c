/* Metadata management functions.
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"
#include "metadata.h"
#include "constant.h"
#include "memory.h"
#include "interval.h"
#include "varray.h"
#include "fh.h"
#include "volume.h"
#include "config.h"
#include "fibheap.h"

/* Data for file descriptor.  */
typedef struct metadata_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
  fibnode heap_node;		/* node of heap whose data is this structure  */
} metadata_fd_data_t;

/* The array of data for each file descriptor.  */
metadata_fd_data_t *metadata_fd_data;

/* Array of opened metadata file descriptors.  */
static fibheap metadata;

/* Mutex protecting access to METADATA.  */
static pthread_mutex_t metadata_mutex;

/* Build path to file with interval tree of purpose PURPOSE for file handle FH
   on volume VOL, the depth of metadata directory tree is TREE_DEPTH.  */

static char *
build_metadata_path (volume vol, internal_fh fh, interval_tree_purpose purpose,
		     unsigned int tree_depth)
{
  char name[17];
  char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
  char *path;
  varray v;
  unsigned int i;

#ifdef ENABLE_CHECKING
  if (tree_depth > MAX_METADATA_TREE_DEPTH)
    abort ();
#endif

  sprintf (name, "%08X%08X", fh->local_fh.dev, fh->local_fh.ino);
#ifdef ENABLE_CHECKING
  if (name[16] != 0)
    abort ();
#endif

  for (i = 0; i < tree_depth; i++)
    {
      tree[2 * i] = name[15 - i];
      tree[2 * i + 1] = '/';
    }
  tree[2 * tree_depth] = 0;

  varray_create (&v, sizeof (char *), 4);
  VARRAY_PUSH (v, vol->local_path, char *);
  switch (purpose)
    {
      case INTERVAL_TREE_UPDATED:
	VARRAY_PUSH (v, "/.zfs/updated/", char *);
	break;

      case INTERVAL_TREE_MODIFIED:
	VARRAY_PUSH (v, "/.zfs/modified/", char *);
	break;
    }
  VARRAY_PUSH (v, tree, char *);
  VARRAY_PUSH (v, name, char *);

  path = xstrconcat_varray (&v);
  varray_destroy (&v);

  return path;
}

/* Create a full path to file FILE with access rights MODE.
   Return true if path exists at the end of this function.  */

static bool
create_path_for_file (char *file, unsigned int mode)
{
  struct stat st;
  char *last;
  char *end;

  for (last = file; *last; last++)
    ;
  last--;

  /* Find the first existing directory.  */
  for (end = last;;)
    {
      for (; end != file && *end != '/'; end--)
	;
      if (end == file)
	return false;

      *end = 0;

      if (lstat (file, &st) == 0)
	{
	  if ((st.st_mode & S_IFMT) != S_IFDIR)
	    return false;

	  break;
	}
    }

  /* Create the path.  */
  for (;;)
    {
      *end = '/';

      if (mkdir (file, mode) != 0)
	return false;

      for (end++; end < last && *end; end++)
	;
      if (end >= last)
	return true;
    }

  return false;
}

/* Flush interval tree TREE to file name PATH.  */

static bool
flush_interval_tree_1 (interval_tree tree, char *path)
{
  char *new_path;
  int fd;

  CHECK_MUTEX_LOCKED (tree->mutex);

  new_path = xstrconcat (2, path, ".new");
  fd = open (new_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
  if (fd < 0)
    {
      free (new_path);
      free (path);
      return false;
    }

  if (!interval_tree_write (tree, fd))
    {
      close (fd);
      unlink (new_path);
      free (new_path);
      free (path);
      return false;
    }

  close (fd);	/* FIXME: do not close but set the info about fd in the tree*/
  rename (new_path, path);

  free (new_path);
  free (path);
  return true;
}

/* Initialize interval tree of purpose PURPOSE for file handle FH
   on volume VOL.  */

bool
init_interval_tree (volume vol, internal_fh fh, interval_tree_purpose purpose)
{
  unsigned int i;
  int fd;
  char *path;
  struct stat st;
  interval_tree *treep;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
  
  path = build_metadata_path (vol, fh, purpose, metadata_tree_depth);
  fd = open (path, O_RDONLY);
  if (fd < 0)
    {
      if (errno != ENOENT)
	{
	  free (path);
	  return false;
	}

      if (!create_path_for_file (path, S_IRWXU))
	{
	  free (path);
	  return false;
	}

      for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
	if (i != metadata_tree_depth)
	  {
	    char *old_path = build_metadata_path (vol, fh, purpose, i);
	    rename (old_path, path);
	  }

      fd = open (path, O_RDONLY);
    }

  switch (purpose)
    {
      case INTERVAL_TREE_UPDATED:
	treep = &fh->updated;
	break;

      case INTERVAL_TREE_MODIFIED:
	treep = &fh->modified;
	break;
    }

  if (fd < 0)
    {
      if (errno != ENOENT)
	{
	  free (path);
	  return false;
	}

      *treep = interval_tree_create (62, &fh->mutex);
    }
  else
    {
      if (fstat (fd, &st) < 0)
	{
	  message (2, stderr, "%s: fstat: %s\n", path, strerror (errno));
	  close (fd);
	  free (path);
	  return false;
	}

      if ((st.st_mode & S_IFMT) != S_IFREG)
	{
	  message (2, stderr, "%s: Not a regular file\n", path);
	  close (fd);
	  free (path);
	  return false;
	}

      if (st.st_size % sizeof (interval) != 0)
	{
	  message (2, stderr, "%s: Interval list is not aligned\n", path);
	  close (fd);
	  free (path);
	  return false;
	}

      *treep = interval_tree_create (62, &fh->mutex);
      if (!interval_tree_read (*treep, fd, st.st_size / sizeof (interval)))
	{
	  interval_tree_destroy (*treep);
	  *treep = NULL;
	  close (fd);
	  free (path);
	  return false;
	}

      close (fd);
    }

  return flush_interval_tree_1 (*treep, path);
}

/* Flush the interval tree of purpose PURPOSE for file handle FH on volume VOL
   to file.  */

bool
flush_interval_tree (volume vol, internal_fh fh, interval_tree_purpose purpose)
{
  char *path;
  interval_tree tree;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  switch (purpose)
    {
      case INTERVAL_TREE_UPDATED:
	tree = fh->updated;
	break;

      case INTERVAL_TREE_MODIFIED:
	tree = fh->modified;
	break;
    }

  path = build_metadata_path (vol, fh, purpose, metadata_tree_depth);
  /* TODO: Close fd if opened.  */
  return flush_interval_tree_1 (tree, path);
}

/* Initialize data structures in METADATA.C.  */

void
initialize_metadata_c ()
{
  int i;

  zfsd_mutex_init (&metadata_mutex);
  metadata = fibheap_new (max_metadata_fds, &metadata_mutex);

  /* Data for each file descriptor.  */
  metadata_fd_data
    = (metadata_fd_data_t *) xcalloc (max_nfd, sizeof (metadata_fd_data_t));
  for (i = 0; i < max_nfd; i++)
    {
      zfsd_mutex_init (&metadata_fd_data[i].mutex);
      metadata_fd_data[i].fd = -1;
    }
}

/* Destroy data structures in METADATA.C.  */

void
cleanup_metadata_c ()
{
  while (fibheap_size (metadata) > 0)
    {
      metadata_fd_data_t *fd_data;

      zfsd_mutex_lock (&metadata_mutex);
      fd_data = (metadata_fd_data_t *) fibheap_min (metadata);
#ifdef ENABLE_CHECKING
      if (!fd_data && fibheap_size (metadata) > 0)
	abort ();
#endif
      if (fd_data && fd_data->fd >= 0)
	/* FIXME: close_local_fd (fd_data->fd) */;
      zfsd_mutex_unlock (&metadata_mutex);
    }
  zfsd_mutex_lock (&metadata_mutex);
  fibheap_delete (metadata);
  zfsd_mutex_unlock (&metadata_mutex);
  zfsd_mutex_destroy (&metadata_mutex);
  free (metadata_fd_data);
}
