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
#include "hardlink-list.h"
#include "journal.h"
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

/* Mutex protecting access to data structures used for opening/closing
   metadata files.  */
static pthread_mutex_t metadata_mutex;

/* Hash function for metadata M.  */
#define METADATA_HASH(M) (crc32_update (crc32_buffer (&(M).dev,		  \
						      sizeof (uint32_t)), \
					&(M).ino, sizeof (uint32_t)))

/* Hash function for file handle mapping M.  */
#define FH_MAPPING_HASH(M) (crc32_update (crc32_buffer (&(M).master_fh.dev, \
							sizeof (uint32_t)), \
					  &(M).master_fh.ino,		    \
					  sizeof (uint32_t)))
static bool init_metadata_for_created_volume_root (volume vol);
static void read_hardlinks_file (hardlink_list sl, int fd);
static void delete_hardlinks_file (volume vol, zfs_fh *fh);
static bool write_hardlinks (volume vol, zfs_fh *fh, hardlink_list hl);

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

#if BYTE_ORDER != LITTLE_ENDIAN

/* Decode element X of the hash file.  */

void
metadata_decode (void *x)
{
  metadata *m = (metadata *) x;

  m->flags = le_to_u32 (m->flags);
  m->dev = le_to_u32 (m->dev);
  m->ino = le_to_u32 (m->ino);
  m->gen = le_to_u32 (m->gen);
  m->master_fh.sid = le_to_u32 (m->master_fh.sid);
  m->master_fh.vid = le_to_u32 (m->master_fh.vid);
  m->master_fh.dev = le_to_u32 (m->master_fh.dev);
  m->master_fh.ino = le_to_u32 (m->master_fh.ino);
  m->master_fh.gen = le_to_u32 (m->master_fh.gen);
  m->local_version = le_to_u64 (m->local_version);
  m->master_version = le_to_u64 (m->master_version);
  m->parent_dev = le_to_u32 (m->parent_dev);
  m->parent_ino = le_to_u32 (m->parent_ino);
}

/* Encode element X of the hash file.  */

void
metadata_encode (void *x)
{
  metadata *m = (metadata *) x;

  m->flags = u32_to_le (m->flags);
  m->dev = u32_to_le (m->dev);
  m->ino = u32_to_le (m->ino);
  m->gen = u32_to_le (m->gen);
  m->master_fh.sid = u32_to_le (m->master_fh.sid);
  m->master_fh.vid = u32_to_le (m->master_fh.vid);
  m->master_fh.dev = u32_to_le (m->master_fh.dev);
  m->master_fh.ino = u32_to_le (m->master_fh.ino);
  m->master_fh.gen = u32_to_le (m->master_fh.gen);
  m->local_version = u64_to_le (m->local_version);
  m->master_version = u64_to_le (m->master_version);
  m->parent_dev = u32_to_le (m->parent_dev);
  m->parent_ino = u32_to_le (m->parent_ino);
}

#endif

/* Hash function for fh_mapping X.  */

static hashval_t
fh_mapping_hash (const void *x)
{
  return FH_MAPPING_HASH (*(fh_mapping *) x);
}

/* Compare element X of hash file with possible element Y.  */

static int
fh_mapping_eq (const void *x, const void *y)
{
  fh_mapping *m1 = (fh_mapping *) x;
  fh_mapping *m2 = (fh_mapping *) y;

  return (m1->master_fh.dev == m2->master_fh.dev
	  && m1->master_fh.ino == m2->master_fh.ino);
}

#if BYTE_ORDER != LITTLE_ENDIAN

/* Decode element X of the hash file.  */

static void
fh_mapping_decode (void *x)
{
  fh_mapping *m = (fh_mapping *) x;

  m->master_fh.sid = le_to_u32 (m->master_fh.sid);
  m->master_fh.vid = le_to_u32 (m->master_fh.vid);
  m->master_fh.dev = le_to_u32 (m->master_fh.dev);
  m->master_fh.ino = le_to_u32 (m->master_fh.ino);
  m->master_fh.gen = le_to_u32 (m->master_fh.gen);
  m->local_fh.sid = le_to_u32 (m->local_fh.sid);
  m->local_fh.vid = le_to_u32 (m->local_fh.vid);
  m->local_fh.dev = le_to_u32 (m->local_fh.dev);
  m->local_fh.ino = le_to_u32 (m->local_fh.ino);
  m->local_fh.gen = le_to_u32 (m->local_fh.gen);
}

/* Encode element X of the hash file.  */

static void
fh_mapping_encode (void *x)
{
  fh_mapping *m = (fh_mapping *) x;

  m->master_fh.sid = u32_to_le (m->master_fh.sid);
  m->master_fh.vid = u32_to_le (m->master_fh.vid);
  m->master_fh.dev = u32_to_le (m->master_fh.dev);
  m->master_fh.ino = u32_to_le (m->master_fh.ino);
  m->master_fh.gen = u32_to_le (m->master_fh.gen);
  m->local_fh.sid = le_to_u32 (m->local_fh.sid);
  m->local_fh.vid = le_to_u32 (m->local_fh.vid);
  m->local_fh.dev = le_to_u32 (m->local_fh.dev);
  m->local_fh.ino = le_to_u32 (m->local_fh.ino);
  m->local_fh.gen = le_to_u32 (m->local_fh.gen);
}

#else
#define fh_mapping_decode NULL
#define fh_mapping_encode NULL
#endif

/* Build path to file with global metadata of type TYPE for volume VOL.  */

static char *
build_metadata_path (volume vol, metadata_type type)
{
  char *path;

#ifdef ENABLE_CHECKING
  if (vol->local_path == NULL)
    abort ();
#endif

  switch (type)
    {
      case METADATA_TYPE_METADATA:
	path = xstrconcat (2, vol->local_path, "/.zfs/metadata");
	break;

      case METADATA_TYPE_FH_MAPPING:
	path = xstrconcat (2, vol->local_path, "/.zfs/fh_mapping");
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
  char name[2 * 8 + 1];
  char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
  char *path;
  varray v;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (vol->local_path == NULL)
    abort ();
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

  varray_create (&v, sizeof (char *), 5);
  VARRAY_PUSH (v, vol->local_path, char *);
  VARRAY_PUSH (v, "/.zfs/", char *);
  VARRAY_PUSH (v, tree, char *);
  VARRAY_PUSH (v, name, char *);
  switch (type)
    {
      case METADATA_TYPE_UPDATED:
	VARRAY_PUSH (v, ".updated", char *);
	break;

      case METADATA_TYPE_MODIFIED:
	VARRAY_PUSH (v, ".modified", char *);
	break;

      case METADATA_TYPE_HARDLINKS:
	VARRAY_PUSH (v, ".hardlinks", char *);
	break;

      case METADATA_TYPE_JOURNAL:
	VARRAY_PUSH (v, ".journal", char *);
	break;

      default:
	abort ();
    }

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

/* Remove file FILE and its path upto depth TREE_DEPTH if it is empty.  */

static bool
remove_file_and_path (char *file, unsigned int tree_depth)
{
  char *end;

  if (unlink (file) < 0 && errno != ENOENT)
    return false;

  for (end = file; *end; end++)
    ;
  for (; tree_depth > 0; tree_depth--)
    {
      while (*end != '/')
	end--;
      *end = 0;

      if (rmdir (file) < 0)
	{
	  if (errno == ENOENT || errno == ENOTEMPTY)
	    return true;
	  return false;
	}
    }

  return true;
}

/* Is the hash file HFILE opened?  */

static bool
hashfile_opened_p (hfile_t hfile)
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

/* Is the file for journal JOURNAL opened?  */

static bool
journal_opened_p (journal_t journal)
{
  CHECK_MUTEX_LOCKED (journal->mutex);

  if (journal->fd < 0)
    return false;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[journal->fd].mutex);
  if (journal->generation != metadata_fd_data[journal->fd].generation)
    {
      zfsd_mutex_unlock (&metadata_fd_data[journal->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      return false;
    }

  metadata_fd_data[journal->fd].heap_node
    = fibheap_replace_key (metadata_heap,
			   metadata_fd_data[journal->fd].heap_node,
			   (fibheapkey_t) time (NULL));
  zfsd_mutex_unlock (&metadata_mutex);
  return true;
}

/* Initialize file descriptor for hash file HFILE.  */

static void
init_hashfile_fd (hfile_t hfile)
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

/* Initialize file descriptor for journal JOURNAL.  */

static void
init_journal_fd (journal_t journal)
{
#ifdef ENABLE_CHECKING
  if (journal->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (journal->mutex);
  CHECK_MUTEX_LOCKED (&metadata_mutex);
  CHECK_MUTEX_LOCKED (&metadata_fd_data[journal->fd].mutex);

  metadata_fd_data[journal->fd].fd = journal->fd;
  metadata_fd_data[journal->fd].generation++;
  journal->generation = metadata_fd_data[journal->fd].generation;
  metadata_fd_data[journal->fd].heap_node
    = fibheap_insert (metadata_heap, (fibheapkey_t) time (NULL),
		      &metadata_fd_data[journal->fd]);
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
  if (fd < 0 && ((flags & O_ACCMODE) != O_RDONLY))
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

/* Open and initialize file descriptor for hash file HFILE of type TYPE.  */

static int
open_hash_file (volume vol, metadata_type type)
{
  hfile_t hfile;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  switch (type)
    {
      case METADATA_TYPE_METADATA:
	hfile = vol->metadata;
	break;

      case METADATA_TYPE_FH_MAPPING:
	hfile = vol->fh_mapping;
	break;

      default:
	abort ();
    }

  fd = open_metadata (hfile->file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0)
    return fd;

  hfile->fd = fd;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  init_hashfile_fd (hfile);
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
  fd = open_fh_metadata (path, vol, &fh->local_fh, type,
			 O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  free (path);
  if (fd < 0)
    return fd;

  if (lseek (fd, 0, SEEK_END) == (off_t) -1)
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

/* Open and initialize file descriptor for journal JOURNAL for file handle FH
   on volume VOL.  */

static int
open_journal_file (volume vol, internal_fh fh)
{
  char *path;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  path = build_fh_metadata_path (vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
				 metadata_tree_depth);
  fd = open_fh_metadata (path, vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
			 O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  free (path);
  if (fd < 0)
    return fd;

  if (lseek (fd, 0, SEEK_END) == (off_t) -1)
    {
      message (1, stderr, "lseek: %s\n", strerror (errno));
      close (fd);
      return -1;
    }

  fh->journal->fd = fd;

  zfsd_mutex_lock (&metadata_mutex);
  zfsd_mutex_lock (&metadata_fd_data[fd].mutex);
  init_journal_fd (fh->journal);
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
	      vol->delete_p = true;

	    if (!remove_file_and_path (path, metadata_tree_depth))
	      vol->delete_p = true;

	    return true;
	  }
	else
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags & ~METADATA_COMPLETE))
	      vol->delete_p = true;
	  }
	break;

      case METADATA_TYPE_MODIFIED:
	if (tree->size == 0)
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags & ~METADATA_MODIFIED))
	      vol->delete_p = true;

	    if (!remove_file_and_path (path, metadata_tree_depth))
	      vol->delete_p = true;

	    return true;
	  }
	else
	  {
	    if (!set_metadata_flags (vol, fh,
				     fh->meta.flags | METADATA_MODIFIED))
	      vol->delete_p = true;
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
  fd = open_fh_metadata (new_path, vol, &fh->local_fh, type,
			 O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

  if (fd < 0)
    {
      free (new_path);
      free (path);
      return false;
    }

  if (!interval_tree_write (tree, fd))
    {
      close (fd);
      remove_file_and_path (new_path, metadata_tree_depth);
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
  if (vol->local_path == NULL)
    abort ();
#endif

  path = build_metadata_path (vol, METADATA_TYPE_METADATA);
  vol->metadata = hfile_create (sizeof (metadata), 32, metadata_hash,
				metadata_eq, metadata_decode, metadata_encode,
				path, &vol->mutex);
  insert_volume_root = (lstat (vol->local_path, &st) < 0);

  if (!create_path_for_file (path, S_IRWXU))
    {
      free (path);
      close_volume_metadata (vol);
      return false;
    }
  free (path);

  fd = open_hash_file (vol, METADATA_TYPE_METADATA);
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
	      unlink (vol->metadata->file_name);
	      close_volume_metadata (vol);
	      return false;
	    }

	  if (ftruncate (fd, ((uint64_t) vol->metadata->size
			      * sizeof (metadata)
			      + sizeof (header))) < 0)
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	      unlink (vol->metadata->file_name);
	      close_volume_metadata (vol);
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
    {
      if (!init_metadata_for_created_volume_root (vol))
	{
	  close_volume_metadata (vol);
	  return false;
	}
    }

  path = build_metadata_path (vol, METADATA_TYPE_FH_MAPPING);
  vol->fh_mapping = hfile_create (sizeof (fh_mapping), 32, fh_mapping_hash,
				  fh_mapping_eq, fh_mapping_decode,
				  fh_mapping_encode, path, &vol->mutex);
  free (path);

  fd = open_hash_file (vol, METADATA_TYPE_FH_MAPPING);
  if (fd < 0)
    {
      close_volume_metadata (vol);
      return false;
    }

  if (fstat (fd, &st) < 0)
    {
      message (2, stderr, "%s: fstat: %s\n", vol->fh_mapping->file_name,
	       strerror (errno));
      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  if (!hfile_init (vol->fh_mapping, &st))
    {
      if ((st.st_mode & S_IFMT) != S_IFREG)
	{
	  message (2, stderr, "%s: Not a regular file\n",
		   vol->fh_mapping->file_name);
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
	      unlink (vol->fh_mapping->file_name);
	      close_volume_metadata (vol);
	      return false;
	    }

	  if (ftruncate (fd, ((uint64_t) vol->fh_mapping->size
			      * sizeof (fh_mapping)
			      + sizeof (header))) < 0)
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[fd].mutex);
	      unlink (vol->fh_mapping->file_name);
	      close_volume_metadata (vol);
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

  return true;
}

/* Close file for hahs file HFILE.  */

static void
close_hash_file (hfile_t hfile)
{
  CHECK_MUTEX_LOCKED (hfile->mutex);

  if (hfile->fd >= 0)
    {
      zfsd_mutex_lock (&metadata_mutex);
      zfsd_mutex_lock (&metadata_fd_data[hfile->fd].mutex);
      if (hfile->generation == metadata_fd_data[hfile->fd].generation)
	close_metadata_fd (hfile->fd);
      else
	zfsd_mutex_unlock (&metadata_fd_data[hfile->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      hfile->fd = -1;
    }
}

/* Close hash file containing metadata for volume VOL.  */

void
close_volume_metadata (volume vol)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (vol->metadata)
    {
      close_hash_file (vol->metadata);
      hfile_destroy (vol->metadata);
      vol->metadata = NULL;
    }
  if (vol->fh_mapping)
    {
      close_hash_file (vol->fh_mapping);
      hfile_destroy (vol->fh_mapping);
      vol->fh_mapping = NULL;
    }
  vol->delete_p = true;
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

/* Close file for journal JOURNAL.  */

void
close_journal_file (journal_t journal)
{
  CHECK_MUTEX_LOCKED (journal->mutex);

  if (journal->fd >= 0)
    {
      zfsd_mutex_lock (&metadata_mutex);
      zfsd_mutex_lock (&metadata_fd_data[journal->fd].mutex);
      if (journal->generation == metadata_fd_data[journal->fd].generation)
	close_metadata_fd (journal->fd);
      else
	zfsd_mutex_unlock (&metadata_fd_data[journal->fd].mutex);
      zfsd_mutex_unlock (&metadata_mutex);
      journal->fd = -1;
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

  if (!hashfile_opened_p (vol->metadata))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_METADATA);
      if (fd < 0)
	return false;
    }

  meta.dev = st.st_dev;
  meta.ino = st.st_ino;
  if (!hfile_lookup (vol->metadata, &meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      return false;
    }

  if (meta.slot_status != VALID_SLOT)
    {
      meta.slot_status = VALID_SLOT;
      meta.flags = 0;
      meta.dev = st.st_dev;
      meta.ino = st.st_ino;
      meta.gen = 1;
      meta.local_version = 1;
      meta.master_version = 1;
      zfs_fh_undefine (meta.master_fh);
      meta.parent_dev = (uint32_t) -1;
      meta.parent_ino = (uint32_t) -1;
      memset (meta.name, 0, METADATA_NAME_SIZE);

      if (!hfile_insert (vol->metadata, &meta))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return false;
	}
    }

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
  return true;
}

/* Lookup metadata for file handle FH on volume VOL.  Store the metadata to META
   and update FH->GEN.  Insert the metadata to hash file if INSERT is true and
   the metadata was not found.  */

static bool
lookup_metadata (volume vol, zfs_fh *fh, metadata *meta, bool insert)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (!vol->metadata)
    abort ();
  if (!vol->local_path)
    abort ();
#endif

  if (!hashfile_opened_p (vol->metadata))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_METADATA);
      if (fd < 0)
	return false;
    }

  meta->dev = fh->dev;
  meta->ino = fh->ino;
  if (!hfile_lookup (vol->metadata, meta))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
      return false;
    }

  if (meta->slot_status != VALID_SLOT)
    {
      meta->slot_status = VALID_SLOT;
      meta->flags = METADATA_COMPLETE;
      meta->dev = fh->dev;
      meta->ino = fh->ino;
      meta->gen = 1;
      meta->local_version = 1;
      meta->master_version = 0;
      zfs_fh_undefine (meta->master_fh);
      meta->parent_dev = (uint32_t) -1;
      meta->parent_ino = (uint32_t) -1;
      memset (meta->name, 0, METADATA_NAME_SIZE);

      if (insert)
	{
	  if (!hfile_insert (vol->metadata, meta))
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	      return false;
	    }
	}
    }
  fh->gen = meta->gen;

  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
  return true;
}

/* Get metadata for file handle FH on volume VOL.
   Store the metadata to META and update FH->GEN.  Unlock the volume.  */

bool
get_metadata (volume vol, zfs_fh *fh, metadata *meta)
{
  if (!vol)
    return false;
#ifdef ENABLE_CHECKING
  if (!meta)
    abort ();
#endif

  if (!lookup_metadata (vol, fh, meta, true))
    {
      vol->delete_p = true;
      close_volume_metadata (vol);
      zfsd_mutex_unlock (&vol->mutex);
      return false;
    }

  zfsd_mutex_unlock (&vol->mutex);
  return true;
}

/* Get file handle mapping for master file handle MASTER_FH on volume VOL
   and store it to MAP.  */

bool
get_fh_mapping_for_master_fh (volume vol, zfs_fh *master_fh, fh_mapping *map)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (!hashfile_opened_p (vol->fh_mapping))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_FH_MAPPING);
      if (fd < 0)
	return false;
    }

  map->master_fh.dev = master_fh->dev;
  map->master_fh.ino = master_fh->ino;
  if (!hfile_lookup (vol->fh_mapping, map))
    {
      zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
      close_volume_metadata (vol);
      return false;
    }

  if (map->slot_status == VALID_SLOT
      && map->master_fh.gen < master_fh->gen)
    {
      /* There is a master file handle with older genration in the hash file
	 so delete it and return undefined local file handle.  */
      if (!hfile_delete (vol->fh_mapping, map))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
	  close_volume_metadata (vol);
	  return false;
	}
      map->slot_status = DELETED_SLOT;
    }

#ifdef ENABLE_VALGRIND_CHECKING
  if (map->slot_status != VALID_SLOT)
    {
      VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (&map->local_fh,
						sizeof (zfs_fh)));
      VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (&map->master_fh,
						sizeof (zfs_fh)));
    }
#endif

  zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
  return true;
}

/* Write the metadata for file handle FH on volume VOL to list file.
   Return false on file error.  */

bool
flush_metadata (volume vol, internal_fh fh)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (!hashfile_opened_p (vol->metadata))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_METADATA);
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

/* Set master_fh to MASTER_FH in metadata for file handle FH on volume VOL
   and update reverse file handle mapping.  */

bool
set_metadata_master_fh (volume vol, internal_fh fh, zfs_fh *master_fh)
{
  fh_mapping map;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (ZFS_FH_EQ (fh->meta.master_fh, *master_fh))
    return true;

  if (!hashfile_opened_p (vol->fh_mapping))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_FH_MAPPING);
      if (fd < 0)
	return false;
    }

  if (fh->meta.master_fh.dev == master_fh->dev
      && fh->meta.master_fh.ino == master_fh->ino)
    {
      map.slot_status = VALID_SLOT;
      map.master_fh = *master_fh;
      map.local_fh = fh->local_fh;
      if (!hfile_insert (vol->fh_mapping, &map))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
	  close_volume_metadata (vol);
	  return false;
	}
    }
  else
    {
      /* Delete original reverse file handle mapping.  */
      map.master_fh.dev = fh->meta.master_fh.dev;
      map.master_fh.ino = fh->meta.master_fh.ino;
      if (!hfile_delete (vol->fh_mapping, &map))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
	  close_volume_metadata (vol);
	  return false;
	}

      /* Set new reverse file handle mapping.  */
      if (!zfs_fh_undefined (*master_fh))
	{
	  map.slot_status = VALID_SLOT;
	  map.master_fh = *master_fh;
	  map.local_fh = fh->local_fh;
	  if (!hfile_insert (vol->fh_mapping, &map))
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);
	      close_volume_metadata (vol);
	      return false;
	    }
	}
    }
  zfsd_mutex_unlock (&metadata_fd_data[vol->fh_mapping->fd].mutex);

  fh->meta.master_fh = *master_fh;
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
   and inode INO and hardlink [PARENT_DEV, PARENT_INO, NAME].  */

bool
delete_metadata (volume vol, uint32_t dev, uint32_t ino,
		 uint32_t parent_dev, uint32_t parent_ino, char *name)
{
  metadata meta;
  zfs_fh fh;
  char *path;
  int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  fh.dev = dev;
  fh.ino = ino;

  /* Delete hardlink.  */
  if (name)
    {
      hardlink_list hl;
      int fd;

      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_HARDLINKS,
				     metadata_tree_depth);
      fd = open_fh_metadata (path, vol, &fh, METADATA_TYPE_HARDLINKS,
			     O_RDONLY, S_IRUSR | S_IWUSR);
      free (path);
      if (fd >= 0)
	{
	  hl = hardlink_list_create (2, NULL);
	  read_hardlinks_file (hl, fd);

	  hardlink_list_delete (hl, parent_dev, parent_ino, name);
	  if (hl->first)
	    return write_hardlinks (vol, &fh, hl);
	  else
	    {
	      hardlink_list_destroy (hl);
	      delete_hardlinks_file (vol, &fh);
	    }
	}
    }

  /* Delete interval files and journal.  */
  for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
    {
      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_UPDATED, i);
      if (!remove_file_and_path (path, i))
	vol->delete_p = true;
      free (path);
      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_MODIFIED, i);
      if (!remove_file_and_path (path, i))
	vol->delete_p = true;
      free (path);
      path = build_fh_metadata_path (vol, &fh, METADATA_TYPE_JOURNAL, i);
      if (!remove_file_and_path (path, i))
	vol->delete_p = true;
      free (path);
    }

  /* Update metadata.  */
  if (!hashfile_opened_p (vol->metadata))
    {
      int fd;

      fd = open_hash_file (vol, METADATA_TYPE_METADATA);
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
      meta.gen = 1;
      meta.local_version = 1;
      meta.master_version = 0;
      zfs_fh_undefine (meta.master_fh);
      meta.parent_dev = (uint32_t) -1;
      meta.parent_ino = (uint32_t) -1;
      memset (meta.name, 0, METADATA_NAME_SIZE);
    }

  meta.flags = 0;
  meta.gen++;
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

/* Delete list of hardlinks of ZFS file handle FH on volume VOL.  */

static void
delete_hardlinks_file (volume vol, zfs_fh *fh)
{
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
    {
      char *file;

      file = build_fh_metadata_path (vol, fh, METADATA_TYPE_HARDLINKS, i);
      if (!remove_file_and_path (file, metadata_tree_depth))
	vol->delete_p = true;
      free (file);
    }
}

/* Read list of hardlinks from file descriptor FD to hardlink list HL.  */

static void
read_hardlinks_file (hardlink_list hl, int fd)
{
  FILE *f;

  f = fdopen (fd, "rb");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  for (;;)
    {
      uint32_t parent_dev;
      uint32_t parent_ino;
      uint32_t name_len;
      char *name;

      if (fread (&parent_dev, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fread (&parent_ino, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fread (&name_len, 1, sizeof (uint32_t), f) != sizeof (uint32_t))
	break;

      parent_dev = le_to_u32 (parent_dev);
      parent_ino = le_to_u32 (parent_ino);
      name_len = le_to_u32 (name_len);
      name = (char *) xmalloc (name_len + 1);

      if (fread (name, 1, name_len + 1, f) != name_len + 1)
	{
	  free (name);
	  break;
	}
      name[name_len] = 0;

      hardlink_list_insert (hl, parent_dev, parent_ino, name, false);
    }

  fclose (f);
}

/* Write hardlink list HL for file handle FH on volume VOL to hardlink file.
   Return false on file error.  */

static bool
write_hardlinks_file (volume vol, zfs_fh *fh, hardlink_list hl)
{
  hardlink_list_entry entry;
  char *path, *new_path;
  int fd;
  FILE *f;

  path = build_fh_metadata_path (vol, fh, METADATA_TYPE_HARDLINKS,
				 metadata_tree_depth);
  new_path = xstrconcat (2, path, ".new");
  fd = open_fh_metadata (new_path, vol, fh, METADATA_TYPE_HARDLINKS,
			 O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0)
    {
      free (new_path);
      free (path);
      return false;
    }

  f = fdopen (fd, "wb");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  for (entry = hl->first; entry; entry = entry->next)
    {
      uint32_t parent_dev;
      uint32_t parent_ino;
      unsigned int name_len;

      parent_dev = u32_to_le (entry->parent_dev);
      parent_ino = u32_to_le (entry->parent_ino);
      name_len = u32_to_le (entry->name.len);
      if (fwrite (&parent_dev, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&parent_ino, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&name_len, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || (fwrite (entry->name.str, 1, entry->name.len + 1, f)
	      != entry->name.len + 1))
	{
	  fclose (f);
	  unlink (new_path);
	  free (new_path);
	  free (path);
	  return false;
	}
    }

  fclose (f);
  rename (new_path, path);
  free (new_path);
  free (path);
  return true;
}

/* Read hardlinks for file handle FH on volume VOL to hardlink list HL
   and the metadata to META.  */

static bool
read_hardlinks (volume vol, zfs_fh *fh, metadata *meta, hardlink_list hl)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (!lookup_metadata (vol, fh, meta, false))
    return false;

  if (meta->name[0] == 0)
    {
      char *path;
      int fd;

#ifdef ENABLE_CHECKING
      if (meta->parent_dev != (uint32_t) -1
	  || meta->parent_ino != (uint32_t) -1)
	abort ();
#endif

      path = build_fh_metadata_path (vol, fh, METADATA_TYPE_HARDLINKS,
				     metadata_tree_depth);
      fd = open_fh_metadata (path, vol, fh, METADATA_TYPE_HARDLINKS,
			     O_RDONLY, S_IRUSR | S_IWUSR);
      free (path);

      if (fd >= 0)
	read_hardlinks_file (hl, fd);
    }
  else
    {
#ifdef ENABLE_CHECKING
      if (meta->parent_dev == (uint32_t) -1
	  && meta->parent_ino == (uint32_t) -1)
	abort ();
#endif

      hardlink_list_insert (hl, meta->parent_dev, meta->parent_ino,
			    meta->name, true);
    }

  return true;
}

/* Write the hardlink list HL for file handle FH on volume VOL
   either to hardlink file or to metadata file.
   Return false on file error.  */

static bool
write_hardlinks (volume vol, zfs_fh *fh, hardlink_list hl)
{
  metadata meta;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (hl->first == NULL)
    abort ();
#endif

  if (hl->first
      && (hl->first->next
	  || hl->first->name.len >= METADATA_NAME_SIZE))
    {
      if (!write_hardlinks_file (vol, fh, hl))
	{
	  hardlink_list_destroy (hl);
	  return false;
	}
      hardlink_list_destroy (hl);

      if (!hashfile_opened_p (vol->metadata))
	{
	  int fd;

	  fd = open_hash_file (vol, METADATA_TYPE_METADATA);
	  if (fd < 0)
	    return false;
	}

      meta.dev = fh->dev;
      meta.ino = fh->ino;
      if (!hfile_lookup (vol->metadata, &meta))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return false;
	}
      if (meta.slot_status != VALID_SLOT)
	{
	  meta.slot_status = VALID_SLOT;
	  meta.flags = METADATA_COMPLETE;
	  meta.dev = fh->dev;
	  meta.ino = fh->ino;
	  meta.gen = 1;
	  meta.local_version = 1;
	  meta.master_version = 0;
	  zfs_fh_undefine (meta.master_fh);
	  meta.parent_dev = (uint32_t) -1;
	  meta.parent_ino = (uint32_t) -1;
	  memset (meta.name, 0, METADATA_NAME_SIZE);

	  if (!hfile_insert (vol->metadata, &meta))
	    {
	      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	      return false;
	    }
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return true;
	}

      if (meta.name[0] == 0)
	{
#ifdef ENABLE_CHECKING
	  if (meta.parent_dev != (uint32_t) -1
	      || meta.parent_ino != (uint32_t) -1)
	    abort ();
#endif
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return true;
	}

#ifdef ENABLE_CHECKING
      if (meta.parent_dev == (uint32_t) -1
	  && meta.parent_ino == (uint32_t) -1)
	abort ();
#endif

      meta.parent_dev = (uint32_t) -1;
      meta.parent_ino = (uint32_t) -1;
      memset (meta.name, 0, METADATA_NAME_SIZE);

      if (!hfile_insert (vol->metadata, &meta))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return false;
	}
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
    }
  else if (hl->first)
    {
      hardlink_list_entry entry;

      if (!hashfile_opened_p (vol->metadata))
	{
	  int fd;

	  fd = open_hash_file (vol, METADATA_TYPE_METADATA);
	  if (fd < 0)
	    {
	      hardlink_list_destroy (hl);
	      return false;
	    }
	}

      meta.dev = fh->dev;
      meta.ino = fh->ino;
      if (!hfile_lookup (vol->metadata, &meta))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  hardlink_list_destroy (hl);
	  return false;
	}
      if (meta.slot_status != VALID_SLOT)
	{
	  meta.slot_status = VALID_SLOT;
	  meta.flags = METADATA_COMPLETE;
	  meta.dev = fh->dev;
	  meta.ino = fh->ino;
	  meta.gen = 1;
	  meta.local_version = 1;
	  meta.master_version = 0;
	  zfs_fh_undefine (meta.master_fh);
	}

      entry = hl->first;
      meta.parent_dev = entry->parent_dev;
      meta.parent_ino = entry->parent_ino;
      memcpy (meta.name, entry->name.str, entry->name.len);
      memset (meta.name + entry->name.len, 0,
	      METADATA_NAME_SIZE - entry->name.len);

      hardlink_list_destroy (hl);

      if (!hfile_insert (vol->metadata, &meta))
	{
	  zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);
	  return false;
	}
      zfsd_mutex_unlock (&metadata_fd_data[vol->metadata->fd].mutex);

      delete_hardlinks_file (vol, fh);
    }
  else
    {
      hardlink_list_destroy (hl);
      delete_hardlinks_file (vol, fh);
    }

  return true;
}

/* Insert a hardlink [PARENT_DEV, PARENT_INO, NAME] to hardlink list for
   file handle FH on volume VOL.
   Return false on file error.  */

bool
metadata_hardlink_insert (volume vol, zfs_fh *fh, uint32_t parent_dev,
			  uint32_t parent_ino, char *name)
{
  hardlink_list hl;
  metadata meta;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  hl = hardlink_list_create (2, NULL);
  if (!read_hardlinks (vol, fh, &meta, hl))
    {
      hardlink_list_destroy (hl);
      return false;
    }

  if (hardlink_list_insert (hl, parent_dev, parent_ino, name, true))
    return write_hardlinks (vol, fh, hl);

  hardlink_list_destroy (hl);
  return true;
}

/* Replace a hardlink [OLD_PARENT_DEV, OLD_PARENT_INO, OLD_NAME] by
   [NEW_PARENT_DEV, NEW_PARENT_INO, NEW_NAME] in hardlink list for file
   handle FH on volume VOL.
   Return false on file error.  */

bool
metadata_hardlink_replace (volume vol, zfs_fh *fh, uint32_t old_parent_dev,
			   uint32_t old_parent_ino, char *old_name,
			   uint32_t new_parent_dev, uint32_t new_parent_ino,
			   char *new_name)
{
  hardlink_list hl;
  metadata meta;
  bool flush;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  hl = hardlink_list_create (2, NULL);
  if (!read_hardlinks (vol, fh, &meta, hl))
    {
      hardlink_list_destroy (hl);
      return false;
    }

  flush = hardlink_list_delete (hl, old_parent_dev, old_parent_ino,
				old_name);
  flush |= hardlink_list_insert (hl, new_parent_dev, new_parent_ino,
				 new_name, true);
  if (flush)
    return write_hardlinks (vol, fh, hl);

  hardlink_list_destroy (hl);
  return true;
}

bool
metadata_hardlink_delete (volume vol, zfs_fh *fh, uint32_t parent_dev,
			  uint32_t parent_ino, char *name)
{
  hardlink_list hl;
  metadata meta;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  hl = hardlink_list_create (2, NULL);
  if (!read_hardlinks (vol, fh, &meta, hl))
    {
      hardlink_list_destroy (hl);
      return false;
    }

  if (hardlink_list_delete (hl, parent_dev, parent_ino, name))
    return write_hardlinks (vol, fh, hl);

  hardlink_list_destroy (hl);
  return true;
}

/* Return a local path for file handle FH on volume VOL.  */

char *
get_local_path_from_metadata (volume vol, zfs_fh *fh)
{
  metadata meta;
  hardlink_list hl;
  hardlink_list_entry entry, next;
  char *parent_path;
  char *path;
  struct stat st;
  bool flush;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  /* Get hardlink list.  */
  hl = hardlink_list_create (2, NULL);
  if (!read_hardlinks (vol, fh, &meta, hl))
    {
      vol->delete_p = true;
      hardlink_list_destroy (hl);
      return NULL;
    }

  /* Check for volume root.  */
  if (meta.parent_dev == (uint32_t) -1
      && meta.parent_ino == (uint32_t) -1
      && meta.name[0] == 0
      && hl->first == NULL)
    {
      hardlink_list_destroy (hl);
      return xstrdup (vol->local_path);
    }

  path = NULL;
  flush = false;
  for (entry = hl->first; entry; entry = next)
    {
      zfs_fh parent_fh;

      next = entry->next;

      parent_fh.dev = entry->parent_dev;
      parent_fh.ino = entry->parent_ino;
      parent_path = get_local_path_from_metadata (vol, &parent_fh);
      if (parent_path == NULL)
	{
	  flush |= hardlink_list_delete (hl, entry->parent_dev,
					 entry->parent_ino, entry->name.str);
	}
      else
	{
	  if (lstat (path, &st) != 0
	      || st.st_dev != fh->dev
	      || st.st_ino != fh->ino)
	    {
	      free (parent_path);
	      flush |= hardlink_list_delete (hl, entry->parent_dev,
					     entry->parent_ino,
					     entry->name.str);
	    }
	  else
	    {
	      path = xstrconcat (3, parent_path, "/", entry->name);
	      free (parent_path);
	      break;
	    }
	}
    }

  if (flush)
    {
      if (!write_hardlinks (vol, fh, hl))
	{
	  vol->delete_p = true;
	  if (path)
	    free (path);
	  return NULL;
	}
    }

  if (hl->first == NULL)
    {
      hardlink_list_destroy (hl);
#ifdef ENABLE_CHECKING
      if (path)
	abort ();
#endif

      if (!delete_metadata (vol, fh->dev, fh->ino, 0, 0, NULL))
	vol->delete_p = true;

      return NULL;
    }

  return path;
}

/* Write the journal for file handle FH on volume VOL to file PATH
   or delete the file if the journal is empty.  */

static bool
flush_journal (volume vol, internal_fh fh, char *path)
{
  journal_entry entry;
  char *new_path;
  int fd;
  FILE *f;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  close_journal_file (fh->journal);

  if (fh->journal->first == NULL)
    {
      bool r;

      r = remove_file_and_path (path, metadata_tree_depth);
      free (path);
      return r;
    }

  new_path = xstrconcat (2, path, ".new");
  fd = open_fh_metadata (new_path, vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
			 O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

  if (fd < 0)
    {
      free (new_path);
      free (path);
      return false;
    }

  f = fdopen (fd, "wb");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  for (entry = fh->journal->first; entry; entry = entry->next)
    {
      uint32_t dev;
      uint32_t ino;
      uint32_t gen;
      uint32_t oper;
      uint32_t name_len;
      zfs_fh master_fh;

      dev = u32_to_le (entry->dev);
      ino = u32_to_le (entry->ino);
      gen = u32_to_le (entry->gen);
      oper = u32_to_le (entry->oper);
      name_len = u32_to_le (entry->name.len);
      master_fh.sid = u32_to_le (entry->master_fh.sid);
      master_fh.vid = u32_to_le (entry->master_fh.vid);
      master_fh.dev = u32_to_le (entry->master_fh.dev);
      master_fh.ino = u32_to_le (entry->master_fh.ino);
      master_fh.gen = u32_to_le (entry->master_fh.gen);
      if (fwrite (&dev, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&ino, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&gen, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&oper, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fwrite (&name_len, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || (fwrite (entry->name.str, 1, entry->name.len + 1, f)
	      != entry->name.len + 1)
	  || (fwrite (&master_fh, 1, sizeof (master_fh), f)
	      != sizeof (master_fh)))
	{
	  fclose (f);
	  unlink (new_path);
	  free (new_path);
	  free (path);
	  return false;
	}
    }

  fclose (f);
  rename (new_path, path);

  free (new_path);
  free (path);
  return true;
}

/* Read journal for file handle FH on volume VOL.  */

bool
read_journal (volume vol, internal_fh fh)
{
  int fd;
  char *path;
  FILE *f;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (!fh->journal)
    abort ();
#endif

  path = build_fh_metadata_path (vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
				 metadata_tree_depth);
  fd = open_fh_metadata (path, vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
			 O_RDONLY, 0);
  if (fd < 0)
    {
      free (path);
      if (errno != ENOENT)
	return false;

      return true;
    }

  f = fdopen (fd, "rb");
#ifdef ENABLE_CHECKING
  if (!f)
    abort ();
#endif

  for (;;)
    {
      zfs_fh local_fh;
      zfs_fh master_fh;
      uint32_t oper;
      uint32_t name_len;
      char *name;

      if (fread (&local_fh.dev, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || (fread (&local_fh.ino, 1, sizeof (uint32_t), f)
	      != sizeof (uint32_t))
	  || (fread (&local_fh.gen, 1, sizeof (uint32_t), f)
	      != sizeof (uint32_t))
	  || fread (&oper, 1, sizeof (uint32_t), f) != sizeof (uint32_t)
	  || fread (&name_len, 1, sizeof (uint32_t), f) != sizeof (uint32_t))
	break;

      local_fh.dev = le_to_u32 (local_fh.dev);
      local_fh.ino = le_to_u32 (local_fh.ino);
      local_fh.gen = le_to_u32 (local_fh.gen);
      oper = le_to_u32 (oper);
      name_len = le_to_u32 (name_len);
      name = (char *) xmalloc (name_len + 1);

      if ((fread (name, 1, name_len + 1, f) != name_len + 1)
	  || (fread (&master_fh, 1, sizeof (master_fh), f)
	      != sizeof (master_fh)))
	{
	  free (name);
	  break;
	}
      name[name_len] = 0;
      master_fh.sid = le_to_u32 (master_fh.sid);
      master_fh.vid = le_to_u32 (master_fh.vid);
      master_fh.dev = le_to_u32 (master_fh.dev);
      master_fh.ino = le_to_u32 (master_fh.ino);
      master_fh.gen = le_to_u32 (master_fh.gen);

      if (oper < JOURNAL_OPERATION_LAST_AND_UNUSED)
	{
	  journal_insert (fh->journal, &local_fh, &master_fh, name,
			  (journal_operation_t) oper, false);
	}
    }

  fclose (f);
  return flush_journal (vol, fh, path);
}

/* Write the journal for file handle FH on volume VOL to appropriate file.  */

bool
write_journal (volume vol, internal_fh fh)
{
  char *path;

  path = build_fh_metadata_path (vol, &fh->local_fh, METADATA_TYPE_JOURNAL,
				 metadata_tree_depth);

  return flush_journal (vol, fh, path);
}

/* Add a journal entry with key [LOCAL_FH, NAME], master file handle MASTER_FH
   and operation OPER to journal for file handle FH on volume VOL.  */

bool
add_journal_entry (volume vol, internal_fh fh, zfs_fh *local_fh,
		   zfs_fh *master_fh, char *name, journal_operation_t oper)
{
  char buffer[DC_SIZE];
  char *end;
  uint32_t len;
  uint32_t tmp32;
  zfs_fh tmp_fh;
  bool r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (!fh->journal)
    abort ();
#endif

  if (!journal_opened_p (fh->journal))
    {
      if (open_journal_file (vol, fh) < 0)
	return false;
    }

  len = strlen (name);
#ifdef ENABLE_CHECKING
  if (len + 1 + 5 * sizeof (uint32_t) + sizeof (master_fh) > DC_SIZE)
    abort ();
#endif

  end = buffer;

  tmp32 = u32_to_le (local_fh->dev);
  memcpy (end, &tmp32, sizeof (uint32_t));
  end += sizeof (uint32_t);

  tmp32 = u32_to_le (local_fh->ino);
  memcpy (end, &tmp32, sizeof (uint32_t));
  end += sizeof (uint32_t);

  tmp32 = u32_to_le (local_fh->gen);
  memcpy (end, &tmp32, sizeof (uint32_t));
  end += sizeof (uint32_t);

  tmp32 = u32_to_le (oper);
  memcpy (end, &tmp32, sizeof (uint32_t));
  end += sizeof (uint32_t);

  tmp32 = u32_to_le (len);
  memcpy (end, &tmp32, sizeof (uint32_t));
  end += sizeof (uint32_t);

  memcpy (end, name, len + 1);
  end += len + 1;

  tmp_fh.sid = u32_to_le (master_fh->sid);
  tmp_fh.vid = u32_to_le (master_fh->vid);
  tmp_fh.dev = u32_to_le (master_fh->dev);
  tmp_fh.ino = u32_to_le (master_fh->ino);
  tmp_fh.gen = u32_to_le (master_fh->gen);
  memcpy (end, &tmp_fh, sizeof (zfs_fh));
  end += sizeof (zfs_fh);

  r = full_write (fh->journal->fd, buffer, end - buffer);
  zfsd_mutex_unlock (&metadata_fd_data[fh->journal->fd].mutex);

  if (!r)
    return false;

  journal_insert (fh->journal, local_fh, master_fh, name, oper, true);

  return true;
}

/* Add a journal entry for file with stat result ST, name NAME
   and operation OPER to journal for file handle FH on volume VOL.  */

bool
add_journal_entry_st (volume vol, internal_fh fh, struct stat *st, char *name,
		      journal_operation_t oper)
{
  metadata meta;
  zfs_fh local_fh;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  local_fh.dev = st->st_dev;
  local_fh.ino = st->st_ino;
  if (!lookup_metadata (vol, &local_fh, &meta, false))
    return false;

  return add_journal_entry (vol, fh, &local_fh, &meta.master_fh, name, oper);
}

/* Initialize data structures in METADATA.C.  */

void
initialize_metadata_c (void)
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
cleanup_metadata_c (void)
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
