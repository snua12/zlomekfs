/* Metadata management functions.
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

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "pthread.h"
#include "metadata.h"
#include "constant.h"
#include "memory.h"
#include "crc32.h"
#include "interval.h"
#include "varray.h"
#include "string-list.h"
#include "fh.h"
#include "volume.h"
#include "config.h"
#include "fibheap.h"
#include "util.h"
#include "data-coding.h"
#include "hashfile.h"
#include "zfs_prot.h"

/* Depth of directory tree for saving metadata about files.  */
unsigned int metadata_tree_depth = 1;

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
static fibheap metadata_heap;

/* Mutex protecting access to METADATA.  */
static pthread_mutex_t metadata_mutex;

/* Hash function for metadata M.  */
#define METADATA_HASH(M) (crc32_update (crc32_buffer (&(M).dev,		  \
						      sizeof (uint32_t)), \
					&(M).ino, sizeof (uint32_t)))

static bool init_metadata_for_created_volume_root (volume vol);
static void delete_hardlinks_fh (volume vol, zfs_fh *fh);
static void read_hardlinks (string_list sl, int fd);
static bool write_hardlinks (volume vol, zfs_fh *fh, string_list sl,
			     char *path);

/* Hash function for metadata X.  */

hashval_t
metadata_hash (const void *x)
{
  return METADATA_HASH (*(metadata *) x);
}

/* Compare element X of hash file with possible element Y.  */

int
metadata_eq (const void *x, const void *y)
{
  metadata *m1 = (metadata *) x;
  metadata *m2 = (metadata *) y;

  return (m1->dev == m2->dev && m1->ino == m2->ino);
}

/* Decode element X of the hash file.  */

void
metadata_decode (void *x)
{
  metadata *m = (metadata *) x;

  m->flags = le_to_u32 (m->flags);
  m->dev = le_to_u32 (m->dev);
  m->ino = le_to_u32 (m->ino);
  m->local_version = le_to_u64 (m->local_version);
  m->master_version = le_to_u64 (m->master_version);
}

/* Encode element X of the hash file.  */

void
metadata_encode (void *x)
{
  metadata *m = (metadata *) x;

  m->flags = u32_to_le (m->flags);
  m->dev = u32_to_le (m->dev);
  m->ino = u32_to_le (m->ino);
  m->local_version = u64_to_le (m->local_version);
  m->master_version = u64_to_le (m->master_version);
}

/* Build path to file with global metadata of type TYPE for volume VOL.  */

static char *
build_metadata_path (volume vol, metadata_type type)
{
  char *path;

  switch (type)
    {
      case METADATA_TYPE_LIST:
	path = xstrconcat (2, vol->local_path, "/.zfs/list");
	break;

      default:
	abort ();
    }

  return path;
}

/* Build path to file with metadata of type TYPE for file handle FH
   on volume VOL, the depth of metadata directory tree is TREE_DEPTH.  */

static char *
build_fh_metadata_path (volume vol, zfs_fh *fh, metadata_type type,
			unsigned int tree_depth)
{
  char name[17];
  char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
  char *path;
  varray v;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (tree_depth > MAX_METADATA_TREE_DEPTH)
    abort ();
#endif

  sprintf (name, "%08X%08X", fh->dev, fh->ino);
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
  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	VARRAY_PUSH (v, "/.zfs/updated/", char *);
	break;

      case METADATA_TYPE_MODIFIED:
	VARRAY_PUSH (v, "/.zfs/modified/", char *);
	break;

      case METADATA_TYPE_HARDLINKS:
	VARRAY_PUSH (v, "/.zfs/hardlinks/", char *);
	break;

      default:
	abort ();
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
  for (last--; last != file && *last != '/'; last--)
    ;
  if (last == file)
    return false;

  *last = 0;

  /* Find the first existing directory.  */
  for (end = last;;)
    {
      if (lstat (file, &st) == 0)
	{
	  if ((st.st_mode & S_IFMT) != S_IFDIR)
	    return false;

	  break;
	}

      for (; end != file && *end != '/'; end--)
	;
      if (end == file)
	return false;

      *end = 0;
    }

  /* Create the path.  */
  for (;;)
    {
      if (end < last)
	{
	  *end = '/';

	  if (mkdir (file, mode) != 0)
	    return false;

	  for (end++; end < last && *end; end++)
	    ;
	}
      if (end >= last)
	{
	  *last = '/';
	  return true;
	}
    }

  return false;
}

/* Is the hash file HFILE for list of file handles opened?  */

static bool
list_opened_p (hfile_t hfile)
{
  CHECK_MUTEX_LOCKED (hfile->mutex);

  if (hfile->fd < 0)
    return false;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[hfile->fd].mutex);
  if (hfile->generation != metadata_fd_data[hfile->fd].generation)
    {
      zfsd_mutex_unlock (&metadata_fd_data[hfile->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      return false;
    }

  metadata_fd_data[hfile->fd].heap_node
    = fibheap_replace_key (metadata_heap, metadata_fd_data[hfile->fd].heap_node,
			   (fibheapkey_t) time (NULL));
  zfsd_mutex_unlock (&metadata_mutex);
  return true;
}

/* Is the interval file for interval tree TREE opened?  */

static bool
interval_opened_p (interval_tree tree)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  if (tree->fd < 0)
    return false;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[tree->fd].mutex);
  if (tree->generation != metadata_fd_data[tree->fd].generation)
    {
      zfsd_mutex_unlock (&metadata_fd_data[tree->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      return false;
    }

  metadata_fd_data[tree->fd].heap_node
    = fibheap_replace_key (metadata_heap, metadata_fd_data[tree->fd].heap_node,
			   (fibheapkey_t) time (NULL));
  zfsd_mutex_unlock (&metadata_mutex);
  return true;
}

/* Initialize file descriptor for hash file HFILE containing list
   of file handles and metadata.  */

static void
init_list_fd (hfile_t hfile)
{
#ifdef ENABLE_CHECKING
  if (hfile->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (hfile->mutex);
  CHECK_MUTEX_LOCKED (&metadata_mutex);
  CHECK_MUTEX_LOCKED (&metadata_fd_data[hfile->fd].mutex);

  metadata_fd_data[hfile->fd].fd = hfile->fd;
  metadata_fd_data[hfile->fd].generation++;
  hfile->generation = metadata_fd_data[hfile->fd].generation;
  metadata_fd_data[hfile->fd].heap_node
    = fibheap_insert (metadata_heap, (fibheapkey_t) time (NULL),
		      &metadata_fd_data[hfile->fd]);
}

/* Initialize file descriptor for interval tree TREE.  */

static void
init_interval_fd (interval_tree tree)
{
#ifdef ENABLE_CHECKING
  if (tree->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (tree->mutex);
  CHECK_MUTEX_LOCKED (&metadata_mutex);
  CHECK_MUTEX_LOCKED (&metadata_fd_data[tree->fd].mutex);

  metadata_fd_data[tree->fd].fd = tree->fd;
  metadata_fd_data[tree->fd].generation++;
  tree->generation = metadata_fd_data[tree->fd].generation;
  metadata_fd_data[tree->fd].heap_node
    = fibheap_insert (metadata_heap, (fibheapkey_t) time (NULL),
		      &metadata_fd_data[tree->fd]);
}

/* Close file descriptor FD of metadata file.  */

static void
close_metadata_fd (int fd)
{
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&metadata_mutex);
  CHECK_MUTEX_LOCKED (&metadata_fd_data[fd].mutex);

#ifdef ENABLE_CHECKING
  if (metadata_fd_data[fd].fd < 0)
    abort ();
#endif
  metadata_fd_data[fd].fd = -1;
  metadata_fd_data[fd].generation++;
  close (fd);
  if (metadata_fd_data[fd].heap_node)
    {
      fibheap_delete_node (metadata_heap, metadata_fd_data[fd].heap_node);
      metadata_fd_data[fd].heap_node = NULL;
    }
  zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
}

/* Open metadata file PATHNAME with open flags FLAGS and mode MODE.  */

static int
open_metadata (const char *pathname, int flags, mode_t mode)
{
  int fd;

retry_open:
  fd = open (pathname, flags, mode);
  if ((fd < 0 && errno == EMFILE)
      || (fd >= 0
	  && fibheap_size (metadata_heap) >= (unsigned int) max_metadata_fds))
    {
      metadata_fd_data_t *fd_data;

      zfsd_mutex_lock (&metadata_mutex);
      fd_data = (metadata_fd_data_t *) fibheap_extract_min (metadata_heap);
#ifdef ENABLE_CHECKING
      if (!fd_data && fibheap_size (metadata_heap) > 0)
	abort ();
#endif
      if (fd_data)
	{
	  zfsd_mutex_lock (&fd_data->mutex);
	  fd_data->heap_node = NULL;
	  if (fd_data->fd >= 0)
	    close_metadata_fd (fd_data->fd);
	  else
	    zfsd_mutex_unlock (&fd_data->mutex);
	}
      zfsd_mutex_unlock (&metadata_mutex);
      if (fd_data)
	goto retry_open;
    }

  return fd;
}

/* Open metadata file of type TYPE for file handle FH on volume VOL
   with path PATH, open flags FLAGS and mode MODE.  */

static int
open_fh_metadata (char *path, volume vol, zfs_fh *fh, metadata_type type,
		  int flags, mode_t mode)
{
  int fd;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  fd = open_metadata (path, flags, mode);
  if (fd < 0)
    {
      if (errno != ENOENT)
	return -1;

      if (!create_path_for_file (path, S_IRWXU))
	{
	  if (errno == ENOENT)
	    errno = 0;
	  return -1;
	}

      for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
	if (i != metadata_tree_depth)
	  {
	    char *old_path;

	    old_path = build_fh_metadata_path (vol, fh, type, i);
	    rename (old_path, path);
	    free (old_path);
	  }

      fd = open_metadata (path, flags, mode);
    }

  return fd;
}

/* Open and initialize file descriptor for hash file HFILE with list
   of file handles and metadata.  */

static int
open_list_file (volume vol)
{
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  fd = open_metadata (vol->metadata->file_name, O_RDWR | O_CREAT,
		      S_IRUSR | S_IWUSR);
  if (fd < 0)
    return fd;

  vol->metadata->fd = fd;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  init_list_fd (vol->metadata);
  zfsd_mutex_unlock (&metadata_mutex);

  return fd;
}

/* Open and initialize file descriptor for interval of type TYPE for
   file handle FH on volume VOL.  */

static int
open_interval_file (volume vol, internal_fh fh, metadata_type type)
{
  interval_tree tree;
  char *path;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  path = build_fh_metadata_path (vol, &fh->local_fh, type,
				 metadata_tree_depth);
  fd = open_metadata (path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  free (path);
  if (fd < 0)
    return fd;

  if (lseek (fd, 0, SEEK_END))
    {
      message (1, stderr, "lseek: %s\n", strerror (errno));
      close (fd);
      return -1;
    }

  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	tree = fh->updated;
	break;

      case METADATA_TYPE_MODIFIED:
	tree = fh->modified;
	break;

      default:
	abort ();
    }

  CHECK_MUTEX_LOCKED (tree->mutex);

  tree->fd = fd;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  init_interval_fd (tree);
  zfsd_mutex_unlock (&metadata_mutex);

  return fd;
}

/* Delete interval file PATH for interval tree TREE of type TYPE
   for internal file handle FH on volume VOL.
   Return true if it was useless.  */

static bool
delete_useless_interval_file (volume vol, internal_fh fh, metadata_type type,
			      interval_tree tree, char *path)
{
  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	if (tree->size == 1
	    && INTERVAL_START (tree->splay->root) == 0
	    && INTERVAL_END (tree->splay->root) == fh->attr.size)
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags | METADATA_COMPLETE))
	      vol->flags |= VOLUME_DELETE;

	    if (unlink (path) < 0 && errno != ENOENT)
	      {
		message (2, stderr, "%s: %s\n", path, strerror (errno));
		vol->flags |= VOLUME_DELETE;
	      }
	    return true;
	  }
	else
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags & ~METADATA_COMPLETE))
	      vol->flags |= VOLUME_DELETE;
	  }
	break;

      case METADATA_TYPE_MODIFIED:
	if (tree->size == 0)
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags & ~METADATA_MODIFIED))
	      vol->flags |= VOLUME_DELETE;

	    if (unlink (path) < 0 && errno != ENOENT)
	      {
		message (2, stderr, "%s: %s\n", path, strerror (errno));
		vol->flags |= VOLUME_DELETE;
	      }
	    return true;
	  }
	else
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags | METADATA_MODIFIED))
	      vol->flags |= VOLUME_DELETE;
	  }
	break;

      default:
	abort ();
    }

  return false;
}

/* Flush interval tree of type TYPE for file handle FH on volume VOL
   to file PATH.  */

static bool
flush_interval_tree_1 (volume vol, internal_fh fh, metadata_type type,
		       char *path)
{
  interval_tree tree;
  char *new_path;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	tree = fh->updated;
	break;

      case METADATA_TYPE_MODIFIED:
	tree = fh->modified;
	break;

      default:
	abort ();
    }

  CHECK_MUTEX_LOCKED (tree->mutex);

  close_interval_file (tree);

  if (delete_useless_interval_file (vol, fh, type, tree, path))
    {
      free (path);
      return true;
    }

  new_path = xstrconcat (2, path, ".new");
  fd = open_metadata (new_path, O_WRONLY | O_TRUNC | O_CREAT,
		      S_IRUSR | S_IWUSR);

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

  rename (new_path, path);
  tree->deleted = false;

#ifdef ENABLE_CHECKING
  if (tree->fd >= 0)
    abort ();
#endif
  tree->fd = fd;
  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[tree->fd].mutex);
  init_interval_fd (tree);
  zfsd_mutex_unlock (&metadata_fd_data[tree->fd].mutex);
  zfsd_mutex_unlock (&metadata_mutex);

  free (new_path);
  free (path);
  return true;
}

/* Initialize hash file containing metadata for volume VOL.  */

bool
init_volume_metadata (volume vol)
{
  hashfile_header header;
  int fd;
  char *path;
  struct stat st;
  bool insert_volume_root;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (!vol->local_path)
    abort ();
#endif

  path = build_metadata_path (vol, METADATA_TYPE_LIST);
  vol->metadata = hfile_create (sizeof (metadata), 256, metadata_hash,
				metadata_eq, metadata_decode, metadata_encode,
				path, &vol->mutex);
  insert_volume_root = (lstat (vol->local_path, &st) < 0);

  if (!create_path_for_file (path, S_IRWXU))
    {
      free (path);
      return false;
    }
  free (path);

  fd = open_list_file (vol);
  if (fd < 0)
    {
      close_volume_metadata (vol);
      return false;
    }

  if (fstat (fd, &st) < 0)
    {
      message (2, stderr, "%s: fstat: %s\n", vol->metadata->file_name,
	       strerror (errno));
      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  if (!hfile_init (vol->metadata, &st))
    {
      if ((st.st_mode & S_IFMT) != S_IFREG)
	{
	  message (2, stderr, "%s: Not a regular file\n",
		   vol->metadata->file_name);
	  zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	  close_volume_metadata (vol);
	  return false;
	}
      else if ((uint64_t) st.st_size < (uint64_t) sizeof (header))
	{
	  header.n_elements = 0;
	  header.n_deleted = 0;
	  if (!full_write (fd, &header, sizeof (header)))
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	      close_volume_metadata (vol);
	      unlink (vol->metadata->file_name);
	      return false;
	    }

	  if (ftruncate (fd, ((uint64_t) vol->metadata->size
			      * sizeof (metadata)
			      + sizeof (header))) < 0)
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	      close_volume_metadata (vol);
	      unlink (vol->metadata->file_name);
	      return false;
	    }
	}
      else
	{
	  zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	  close_volume_metadata (vol);
	  return false;
	}
    }

  zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);

  if (insert_volume_root)
    return init_metadata_for_created_volume_root (vol);
  return true;
}

/* Close hash file containing metadata for volume VOL.  */

void
close_volume_metadata (volume vol)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (vol->metadata->fd >= 0)
    {
      zfsd_mutex_lock (&metadata_mutex);
      zfsd_mutex_lock (&metadata_fd_data[vol->metadata->fd].mutex);
      if (vol->metadata->generation
	  == metadata_fd_data[vol->metadata->fd].generation)
	{
	  close_metadata_fd (vol->metadata->fd);
	}
      else
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	}
      zfsd_mutex_unlock (&metadata_mutex);
    }
  vol->metadata->fd = -1;
  hfile_destroy (vol->metadata);
  vol->metadata = NULL;
  vol->flags |= VOLUME_DELETE;
}

/* Close file for interval tree TREE.  */

void
close_interval_file (interval_tree tree)
{
  CHECK_MUTEX_LOCKED (tree->mutex);

  if (tree->fd >= 0)
    {
      zfsd_mutex_lock (&metadata_mutex);
      zfsd_mutex_lock (&metadata_fd_data[tree->fd].mutex);
      if (tree->generation == metadata_fd_data[tree->fd].generation)
	close_metadata_fd (tree->fd);
      else
	zfsd_mutex_unlock (&metadata_fd_data[tree->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      tree->fd = -1;
    }
}

/* Initialize interval tree of type TYPE for file handle FH on volume VOL.  */

bool
init_interval_tree (volume vol, internal_fh fh, metadata_type type)
{
  int fd;
  char *path;
  struct stat st;
  interval_tree *treep;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	if (fh->meta.flags & METADATA_COMPLETE)
	  {
	    fh->updated = interval_tree_create (62, &fh->mutex);
	    interval_tree_insert (fh->updated, 0, fh->attr.size);
	    return true;
	  }
	treep = &fh->updated;
	break;

      case METADATA_TYPE_MODIFIED:
	if (!(fh->meta.flags & METADATA_MODIFIED))
	  {
	    fh->modified = interval_tree_create (62, &fh->mutex);
	    return true;
	  }
	treep = &fh->modified;
	break;

      default:
	abort ();
    }

  path = build_fh_metadata_path (vol, &fh->local_fh, type,
				 metadata_tree_depth);
  fd = open_fh_metadata (path, vol, &fh->local_fh, type, O_RDONLY, 0);
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

  return flush_interval_tree_1 (vol, fh, type, path);
}

/* Flush the interval tree of type TYPE for file handle FH on volume VOL
   to file.  */

bool
flush_interval_tree (volume vol, internal_fh fh, metadata_type type)
{
  char *path;

  path = build_fh_metadata_path (vol, &fh->local_fh, type,
				 metadata_tree_depth);

  return flush_interval_tree_1 (vol, fh, type, path);
}

/* Flush the interval tree of type TYPE for file handle FH on volume VOL
   to file and free the interval tree.  */

bool
free_interval_tree (volume vol, internal_fh fh, metadata_type type)
{
  char *path;
  interval_tree tree, *treep;
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	tree = fh->updated;
	treep = &fh->updated;
	break;

      case METADATA_TYPE_MODIFIED:
	tree = fh->modified;
	treep = &fh->modified;
	break;

      default:
	abort ();
    }

  CHECK_MUTEX_LOCKED (tree->mutex);

  path = build_fh_metadata_path (vol, &fh->local_fh, type,
				 metadata_tree_depth);

  r = flush_interval_tree_1 (vol, fh, type, path);
  close_interval_file (tree);
  interval_tree_destroy (tree);
  *treep = NULL;

  return r;
}

/* Write the interval [START, END) to the end of interval file of type TYPE
   for file handle FH on volume VOL.  Open the interval file for appending
   when it is not opened.  */

bool
append_interval (volume vol, internal_fh fh, metadata_type type,
		 uint64_t start, uint64_t end)
{
  interval_tree tree;
  interval i;
  char *path;
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	tree = fh->updated;
	break;

      case METADATA_TYPE_MODIFIED:
	tree = fh->modified;
	break;

      default:
	abort ();
    }

  CHECK_MUTEX_LOCKED (tree->mutex);
  interval_tree_insert (tree, start, end);

  if (!interval_opened_p (tree))
    {
      if (open_interval_file (vol, fh, type) < 0)
	return false;
    }

  i.start = u64_to_le (start);
  i.end = u64_to_le (end);
  r = full_write (tree->fd, &i, sizeof (interval));

  zfsd_mutex_unlock (&metadata_fd_data[tree->fd].mutex);

  path = build_fh_metadata_path (vol, &fh->local_fh, type,
				 metadata_tree_depth);
  delete_useless_interval_file (vol, fh, type, tree, path);
  free (path);

  return r;
}

/* Set version in attributes ATTR according to metadata META.  */

void
set_attr_version (fattr *attr, metadata *meta)
{
  attr->version = meta->local_version;
  if (meta->flags & METADATA_MODIFIED)
    attr->version++;
}

/* Init metadata for root of volume VOL according to ST so that volume root
   would be updated.  */

static bool
init_metadata_for_created_volume_root (volume vol)
{
  struct stat st;
  metadata meta;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (lstat (vol->local_path, &st) < 0)
    return false;

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    return false;

  if (!list_opened_p (vol->metadata))
    {
      int fd;

      fd = open_list_file (vol);
      if (fd < 0)
	return false;
    }

  meta.dev = st.st_dev;
  meta.ino = st.st_ino;
  if (!hfile_lookup (vol->metadata, &meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  if (meta.slot_status != VALID_SLOT)
    {
      meta.slot_status = VALID_SLOT;
      meta.flags = 0;
      meta.dev = st.st_dev;
      meta.ino = st.st_ino;
      meta.local_version = 1;
      meta.master_version = 1;
    }

  if (!hfile_insert (vol->metadata, &meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
  return true;
}

/* Init metadata for file handle FH on volume VOL.
   Return false on file error.  */

bool
init_metadata (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (!list_opened_p (vol->metadata))
    {
      int fd;

      fd = open_list_file (vol);
      if (fd < 0)
	return false;
    }

  fh->meta.dev = fh->local_fh.dev;
  fh->meta.ino = fh->local_fh.ino;
  if (!hfile_lookup (vol->metadata, &fh->meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  if (fh->meta.slot_status != VALID_SLOT)
    {
      fh->meta.slot_status = VALID_SLOT;
      fh->meta.flags = METADATA_COMPLETE;
      fh->meta.dev = fh->local_fh.dev;
      fh->meta.ino = fh->local_fh.ino;
      fh->meta.local_version = 0;
      fh->meta.master_version = 0;
    }

  set_attr_version (&fh->attr, &fh->meta);

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
  return true;
}

/* Write the metadata for file handle FH on volume VOL to list file.
   Return false on file error.  */

static bool
flush_metadata (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (!list_opened_p (vol->metadata))
    {
      int fd;

      fd = open_list_file (vol);
      if (fd < 0)
	return false;
    }

  if (!hfile_insert (vol->metadata, &fh->meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
  return true;
}

/* Set metadata (FLAGS, LOCAL_VERSION, MASTER_VERSION) for file handle FH
   on volume VOL.  Return false on file error.  */

bool
set_metadata (volume vol, internal_fh fh, uint32_t flags,
	      uint64_t local_version, uint64_t master_version)
{
  bool modified;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  modified = false;
  if (fh->meta.flags != flags)
    {
      fh->meta.flags = flags;
      modified = true;
    }
  if (fh->meta.local_version != local_version)
    {
      fh->meta.local_version = local_version;
      modified = true;
    }
  if (fh->meta.master_version != master_version)
    {
      fh->meta.master_version = master_version;
      modified = true;
    }

  if (!modified)
    return true;

  set_attr_version (&fh->attr, &fh->meta);

  return flush_metadata (vol, fh);
}

/* Set metadata flags FLAGS for file handle FH on volume VOL.
   Return false on file error.  */

bool
set_metadata_flags (volume vol, internal_fh fh, uint32_t flags)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (fh->meta.flags == flags)
    return true;

  fh->meta.flags = flags;
  set_attr_version (&fh->attr, &fh->meta);

  return flush_metadata (vol, fh);
}

/* Increase the local version for file FH on volume VOL.
   Return false on file error.  */

bool
inc_local_version (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  fh->meta.local_version++;
  set_attr_version (&fh->attr, &fh->meta);

  return flush_metadata (vol, fh);
}

/* Delete all metadata files for file on volume VOL with device DEV
   and inode INO and hardlink HARDLINK.  */

bool
delete_metadata (volume vol, uint32_t dev, uint32_t ino, char *hardlink)
{
  metadata meta;
  zfs_fh fh;
  char *path;
  int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  fh.dev = dev;
  fh.ino = ino;

  for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
    {
      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_UPDATED, i);
      unlink (path);
      free (path);
      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_MODIFIED, i);
      unlink (path);
      free (path);
    }

  if (hardlink)
    {
      string_list sl;
      int fd;

      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_HARDLINKS,
				     metadata_tree_depth);
      fd = open_fh_metadata (path, vol, &fh, METADATA_TYPE_HARDLINKS,
			     O_RDONLY, S_IRUSR | S_IWUSR);
      if (fd >= 0)
	{
	  sl = string_list_create (4, NULL);
	  read_hardlinks (sl, fd);

	  string_list_delete (sl, hardlink);
	  if (string_list_size (sl) >= 2)
	    {
	      if (!write_hardlinks (vol, &fh, sl, path))
		{
		  string_list_destroy (sl);
		  free (path);
		  return false;
		}
	    }
	  else
	    delete_hardlinks_fh (vol, &fh);

	  string_list_destroy (sl);
	}

      free (path);
    }

  if (!list_opened_p (vol->metadata))
    {
      int fd;

      fd = open_list_file (vol);
      if (fd < 0)
	return false;
    }

  meta.dev = dev;
  meta.ino = ino;
  if (!hfile_lookup (vol->metadata, &meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }
  if (meta.slot_status != VALID_SLOT)
    {
      meta.slot_status = VALID_SLOT;
      meta.flags = METADATA_COMPLETE;
      meta.dev = dev;
      meta.ino = ino;
      meta.local_version = 1;
      meta.master_version = 0;
    }

  meta.flags = 0;
  meta.local_version++;
  if (!hfile_insert (vol->metadata, &meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);

  return true;
}

/* Load interval trees for file handle FH on volume VOL.
   Return false on file error.  */

bool
load_interval_trees (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  fh->interval_tree_users++;
  if (fh->interval_tree_users > 1)
    return true;

  if (!init_interval_tree (vol, fh, METADATA_TYPE_UPDATED))
    {
      fh->interval_tree_users--;
      return false;
    }
  if (!init_interval_tree (vol, fh, METADATA_TYPE_MODIFIED))
    {
      fh->interval_tree_users--;
      close_interval_file (fh->updated);
      interval_tree_destroy (fh->updated);
      fh->updated = NULL;
      return false;
    }

  return true;
}

/* Save interval trees for file handle FH on volume VOL.
   Return false on file error.  */

bool
save_interval_trees (volume vol, internal_fh fh)
{
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

#ifdef ENABLE_CHECKING
  if (fh->interval_tree_users == 0)
    abort ();
#endif

  fh->interval_tree_users--;
  if (fh->interval_tree_users > 0)
    return true;

#ifdef ENABLE_CHECKING
  if (!fh->updated)
    abort ();
  if (!fh->modified)
    abort ();
#endif

  r = free_interval_tree (vol, fh, METADATA_TYPE_UPDATED);
  r &= free_interval_tree (vol, fh, METADATA_TYPE_MODIFIED);

  return r;
}

/* Open file with list of hardlinks for file handle FH on volume VOL.
   Return open file descriptor.  */

static int
open_hardlinks_file (volume vol, internal_fh fh)
{
  char *path;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  path = build_fh_metadata_path (vol, &fh->local_fh, METADATA_TYPE_HARDLINKS,
				 metadata_tree_depth);
  fd = open_fh_metadata (path, vol, &fh->local_fh, METADATA_TYPE_HARDLINKS,
			 O_RDONLY, S_IRUSR | S_IWUSR);
  free (path);
  if (fd < 0)
    return fd;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  metadata_fd_data[fd].fd = fd;
  metadata_fd_data[fd].generation++;
  metadata_fd_data[fd].heap_node
    = fibheap_insert (metadata_heap, (fibheapkey_t) time (NULL),
		      &metadata_fd_data[fd]);
  zfsd_mutex_unlock (&metadata_mutex);

  return fd;
}

/* Close file F with descriptor FD and generation GENERATION
   containing list of hardlinks.  */

static void
close_hardlinks_file (int fd, unsigned int generation)
{
  zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  if (generation == metadata_fd_data[fd].generation)
    {
      close_metadata_fd (fd);
    }
  else
    {
      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
    }
  zfsd_mutex_unlock (&metadata_mutex);
}

/* Delete list of hardlinks of ZFS file handle FH on volume VOL.  */

static void
delete_hardlinks_fh (volume vol, zfs_fh *fh)
{
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
    {
      char *file;

      file = build_fh_metadata_path (vol, fh, METADATA_TYPE_HARDLINKS, i);
      unlink (file);
      free (file);
    }
}

/* Delete list of hardlinks of file handle FH on volume VOL.  */

static void
delete_hardlinks_file (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (string_list_size (fh->hardlinks) >= 2)
    abort ();
#endif

  string_list_destroy (fh->hardlinks);
  fh->hardlinks = NULL;
  delete_hardlinks_fh (vol, &fh->local_fh);
}

/* Read list of hardlinks from file descriptor FD to string list SL.  */

static void
read_hardlinks (string_list sl, int fd)
{
  char line[ZFS_MAXPATHLEN + 1];
  FILE *f;

  f = fdopen (fd, "rt");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  while (fgets (line, ZFS_MAXPATHLEN + 1, f))
    {
      unsigned int l;

      l = strlen (line);
      if (l == ZFS_MAXPATHLEN && line[ZFS_MAXPATHLEN - 1] != '\n')
	{
	  /* Read the rest of long line.  */
	  do
	    {
	      if (!fgets (line, ZFS_MAXPATHLEN + 1, f))
		break;
	    }
	  while (l == ZFS_MAXPATHLEN && line[ZFS_MAXPATHLEN - 1] != '\n');
	}
      else if (line[0] == '/')
	{
	  /* Valid line always starts with '/'.  */

	  if (line[l - 1] == '\n')
	    {
	      line[l - 1] = 0;
	      l--;
	    }

	  string_list_insert (sl, line, true);
	}
    }

  fclose (f);
}

/* Write list of hardlinks from string list SL to file PATH.  */

static bool
write_hardlinks (volume vol, zfs_fh *fh, string_list sl, char *path)
{
  char *new_path;
  unsigned int i, n;
  int fd;
  FILE *f;

  new_path = xstrconcat (2, path, ".new");
  fd = open_fh_metadata (new_path, vol, fh, METADATA_TYPE_HARDLINKS,
			 O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0)
    {
      free (new_path);
      return false;
    }

  f = fdopen (fd, "wt");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  n = string_list_size (sl);
  for (i = 0; i < n; i++)
    {
      if (!fputs (string_list_element (sl, i), f)
	  || !fputc ('\n', f))
	{
	  fclose (f);
	  unlink (new_path);
	  free (new_path);
	  return false;
	}
    }

  fclose (f);
  rename (new_path, path);
  free (new_path);
  return true;
}

/* Load list of hardlinks of file handle FH on volume VOL.
   Return false on file error.  */

bool
init_hardlinks (volume vol, internal_fh fh)
{
  unsigned int generation;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (fh->hardlinks)
    abort ();
#endif

  fd = open_hardlinks_file (vol, fh);
  if (fd < 0)
    return true;
  generation = metadata_fd_data[fd].generation;

  fh->hardlinks = string_list_create (4, &fh->mutex);
  read_hardlinks (fh->hardlinks, fd);
  close_hardlinks_file (fd, generation);

  /* There is at most one valid hardlink so delete the list of hardlinks.  */
  if (string_list_size (fh->hardlinks) < 2)
    delete_hardlinks_file (vol, fh);

  return true;
}

/* Write list of hardlinks for file handle FH on volume VOL to file.
   If there is only one hardlink delete the file with the list.
   Return false on file error.  */

bool
flush_hardlinks (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (!fh->hardlinks)
    abort ();
#endif

  if (string_list_size (fh->hardlinks) >= 2)
    {
      char *path;
      bool ret;

      path = build_fh_metadata_path (vol, &fh->local_fh,
				     METADATA_TYPE_HARDLINKS,
				     metadata_tree_depth);
      ret = write_hardlinks (vol, &fh->local_fh, fh->hardlinks, path);
      free (path);
      return ret;
    }
  else
    delete_hardlinks_file (vol, fh);

  return true;
}

/* Initialize data structures in METADATA.C.  */

void
initialize_metadata_c ()
{
  int i;

  zfsd_mutex_init (&metadata_mutex);
  metadata_heap = fibheap_new (max_metadata_fds, &metadata_mutex);

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
  while (fibheap_size (metadata_heap) > 0)
    {
      metadata_fd_data_t *fd_data;

      zfsd_mutex_lock (&metadata_mutex);
      fd_data = (metadata_fd_data_t *) fibheap_extract_min (metadata_heap);
#ifdef ENABLE_CHECKING
      if (!fd_data && fibheap_size (metadata_heap) > 0)
	abort ();
#endif
      if (fd_data)
	{
	  zfsd_mutex_lock (&fd_data->mutex);
	  fd_data->heap_node = NULL;
	  if (fd_data->fd >= 0)
	    close_metadata_fd (fd_data->fd);
	  else
	    zfsd_mutex_unlock (&fd_data->mutex);
	}
      zfsd_mutex_unlock (&metadata_mutex);
    }
  zfsd_mutex_lock (&metadata_mutex);
  fibheap_delete (metadata_heap);
  zfsd_mutex_unlock (&metadata_mutex);
  zfsd_mutex_destroy (&metadata_mutex);
  free (metadata_fd_data);
}
