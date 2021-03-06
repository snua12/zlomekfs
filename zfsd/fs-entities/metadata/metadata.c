/*! \file \brief Metadata management functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "pthread-wrapper.h"
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
#include "configuration.h"
#include "fibheap.h"
#include "util.h"
#include "data-coding.h"
#include "hashfile.h"
#include "zfs-prot.h"
#include "user-group.h"

/*! \brief Data for file descriptor.  */
typedef struct metadata_fd_data_def
{
	pthread_mutex_t mutex;
	int fd;						/*!< file descriptor */
	unsigned int generation;	/*!< generation of open file descriptor */
	fibnode heap_node;			/*!< node of heap whose data is this
								   structure */
} metadata_fd_data_t;

/*! The array of data for each file descriptor.  */
metadata_fd_data_t *metadata_fd_data;

/*! Array of opened metadata file descriptors.  */
static fibheap metadata_heap;

/*! Mutex protecting access to data structures used for opening/closing
   metadata files.  */
static pthread_mutex_t metadata_mutex;

/*! Hash function for metadata M.  */
#define METADATA_HASH(M) (crc32_update (crc32_buffer (&(M).dev,		  \
                                                      sizeof (uint32_t)), \
                                        &(M).ino, sizeof (uint32_t)))

/*! Hash function for file handle mapping M.  */
#define FH_MAPPING_HASH(M) (crc32_update (crc32_buffer (&(M).master_fh.dev, \
                                                        sizeof (uint32_t)), \
                                          &(M).master_fh.ino,		    \
                                          sizeof (uint32_t)))
static bool init_metadata_for_created_volume_root(volume vol);
static void read_hardlinks_file(hardlink_list sl, int fd);
static void delete_hardlinks_file(volume vol, zfs_fh * fh);
static bool write_hardlinks(volume vol, zfs_fh * fh, metadata * meta,
							hardlink_list hl);

bool is_valid_metadata_tree_depth(int depth)
{
	return (depth >= MIN_METADATA_TREE_DEPTH && depth <= MAX_METADATA_TREE_DEPTH); 
}

/*! Hash function for metadata X.  */

hashval_t metadata_hash(const void *x)
{
	return METADATA_HASH(*(const metadata *)x);
}

/*! Compare element X of hash file with possible element Y.  */

int metadata_eq(const void *x, const void *y)
{
	const metadata *m1 = (const metadata *)x;
	const metadata *m2 = (const metadata *)y;

	return (m1->dev == m2->dev && m1->ino == m2->ino);
}

#if BYTE_ORDER != LITTLE_ENDIAN

void zfs_fh_decode(void *x)
{
	zfs_fh *z = (zfs_fh *) x;
	z->sid = le_to_u32(z->sid);
	z->vid = le_to_u32(z->vid);
	z->dev = le_to_u32(z->dev);
	z->ino = le_to_u32(z->ino);
	z->gen = le_to_u32(z->gen);
}

/*! Decode element X of the hash file.  */

void metadata_decode(void *x)
{
	metadata *m = (metadata *) x;

	m->slot_status = le_to_u32(m->slot_status);
	m->flags = le_to_u32(m->flags);
	m->dev = le_to_u32(m->dev);
	m->ino = le_to_u32(m->ino);
	m->gen = le_to_u32(m->gen);

	zfs_fh_decode(&m->master_fh);

	m->local_version = le_to_u64(m->local_version);
	m->master_version = le_to_u64(m->master_version);
	m->modetype = le_to_u32(m->modetype);
	m->uid = le_to_u32(m->uid);
	m->gid = le_to_u32(m->gid);
	m->parent_dev = le_to_u32(m->parent_dev);
	m->parent_ino = le_to_u32(m->parent_ino);
}

/*! Encode element X of the hash file.  */

void metadata_encode(void *x)
{
	metadata *m = (metadata *) x;

	m->flags = u32_to_le(m->flags);
	m->dev = u32_to_le(m->dev);
	m->ino = u32_to_le(m->ino);
	m->gen = u32_to_le(m->gen);
	m->master_fh.sid = u32_to_le(m->master_fh.sid);
	m->master_fh.vid = u32_to_le(m->master_fh.vid);
	m->master_fh.dev = u32_to_le(m->master_fh.dev);
	m->master_fh.ino = u32_to_le(m->master_fh.ino);
	m->master_fh.gen = u32_to_le(m->master_fh.gen);
	m->local_version = u64_to_le(m->local_version);
	m->master_version = u64_to_le(m->master_version);
	m->modetype = u32_to_le(m->modetype);
	m->uid = u32_to_le(m->uid);
	m->gid = u32_to_le(m->gid);
	m->parent_dev = u32_to_le(m->parent_dev);
	m->parent_ino = u32_to_le(m->parent_ino);
}

#endif

/*! Hash function for fh_mapping X.  */

static hashval_t fh_mapping_hash(const void *x)
{
	return FH_MAPPING_HASH(*(const fh_mapping *)x);
}

/*! Compare element X of hash file with possible element Y.  */

static int fh_mapping_eq(const void *x, const void *y)
{
	const fh_mapping *m1 = (const fh_mapping *)x;
	const fh_mapping *m2 = (const fh_mapping *)y;

	return (m1->master_fh.dev == m2->master_fh.dev
			&& m1->master_fh.ino == m2->master_fh.ino);
}

#if BYTE_ORDER != LITTLE_ENDIAN

/*! Decode element X of the hash file.  */

static void fh_mapping_decode(void *x)
{
	fh_mapping *m = (fh_mapping *) x;

	m->master_fh.sid = le_to_u32(m->master_fh.sid);
	m->master_fh.vid = le_to_u32(m->master_fh.vid);
	m->master_fh.dev = le_to_u32(m->master_fh.dev);
	m->master_fh.ino = le_to_u32(m->master_fh.ino);
	m->master_fh.gen = le_to_u32(m->master_fh.gen);
	m->local_fh.sid = le_to_u32(m->local_fh.sid);
	m->local_fh.vid = le_to_u32(m->local_fh.vid);
	m->local_fh.dev = le_to_u32(m->local_fh.dev);
	m->local_fh.ino = le_to_u32(m->local_fh.ino);
	m->local_fh.gen = le_to_u32(m->local_fh.gen);
}

/*! Encode element X of the hash file.  */

static void fh_mapping_encode(void *x)
{
	fh_mapping *m = (fh_mapping *) x;

	m->master_fh.sid = u32_to_le(m->master_fh.sid);
	m->master_fh.vid = u32_to_le(m->master_fh.vid);
	m->master_fh.dev = u32_to_le(m->master_fh.dev);
	m->master_fh.ino = u32_to_le(m->master_fh.ino);
	m->master_fh.gen = u32_to_le(m->master_fh.gen);
	m->local_fh.sid = le_to_u32(m->local_fh.sid);
	m->local_fh.vid = le_to_u32(m->local_fh.vid);
	m->local_fh.dev = le_to_u32(m->local_fh.dev);
	m->local_fh.ino = le_to_u32(m->local_fh.ino);
	m->local_fh.gen = le_to_u32(m->local_fh.gen);
}

#else
#define fh_mapping_decode NULL
#define fh_mapping_encode NULL
#endif

/*! Build path to file with global metadata of type TYPE for volume VOL.  */

static void build_metadata_path(string * path, volume vol, metadata_type type)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (vol->local_path.str == NULL)
		zfsd_abort();
#endif

	switch (type)
	{
	case METADATA_TYPE_METADATA:
		append_string(path, &vol->local_path, "/.zfs/metadata", 14);
		break;

	case METADATA_TYPE_FH_MAPPING:
		append_string(path, &vol->local_path, "/.zfs/fh_mapping", 16);
		break;

	default:
		zfsd_abort();
	}
}

/*! Build path to file with metadata of type TYPE for file handle FH on
   volume VOL, the depth of metadata directory tree is TREE_DEPTH.  */

static void
build_fh_metadata_path(string * path, volume vol, zfs_fh * fh,
					   metadata_type type, unsigned int tree_depth)
{
	char name[3 * 8 + 1];
	char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
	varray v;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (vol->local_path.str == NULL)
		zfsd_abort();
	if (tree_depth > MAX_METADATA_TREE_DEPTH)
		zfsd_abort();
#endif

	if (type == METADATA_TYPE_JOURNAL)
	{
		sprintf(name, "%08X%08X%08X", fh->dev, fh->ino, fh->gen);
#ifdef ENABLE_CHECKING
		if (name[3 * 8] != 0)
			zfsd_abort();
#endif
	}
	else
	{
		sprintf(name, "%08X%08X", fh->dev, fh->ino);
#ifdef ENABLE_CHECKING
		if (name[2 * 8] != 0)
			zfsd_abort();
#endif
	}

	for (i = 0; i < tree_depth; i++)
	{
		tree[2 * i] = name[15 - i];
		tree[2 * i + 1] = '/';
	}
	tree[2 * tree_depth] = 0;

	varray_create(&v, sizeof(string), 5);
	VARRAY_USED(v) = 5;
	VARRAY_ACCESS(v, 0, string) = vol->local_path;
	VARRAY_ACCESS(v, 1, string).str = "/.zfs/";
	VARRAY_ACCESS(v, 1, string).len = 6;
	VARRAY_ACCESS(v, 2, string).str = tree;
	VARRAY_ACCESS(v, 2, string).len = 2 * tree_depth;
	VARRAY_ACCESS(v, 3, string).str = name;
	VARRAY_ACCESS(v, 3, string).len = 2 * 8;
	switch (type)
	{
	case METADATA_TYPE_UPDATED:
		VARRAY_ACCESS(v, 4, string).str = ".updated";
		VARRAY_ACCESS(v, 4, string).len = 8;
		break;

	case METADATA_TYPE_MODIFIED:
		VARRAY_ACCESS(v, 4, string).str = ".modified";
		VARRAY_ACCESS(v, 4, string).len = 9;
		break;

	case METADATA_TYPE_HARDLINKS:
		VARRAY_ACCESS(v, 4, string).str = ".hardlinks";
		VARRAY_ACCESS(v, 4, string).len = 10;
		break;

	case METADATA_TYPE_JOURNAL:
		VARRAY_ACCESS(v, 3, string).len = 3 * 8;
		VARRAY_ACCESS(v, 4, string).str = ".journal";
		VARRAY_ACCESS(v, 4, string).len = 8;
		break;

	default:
		zfsd_abort();
	}

	xstringconcat_varray(path, &v);
	varray_destroy(&v);
}

/*! Build path PATH to shadow file for file FH with name FILE_NAME on volume
   VOL.  */

static void
build_shadow_metadata_path(string * path, volume vol, zfs_fh * fh,
						   string * file_name)
{
#if METADATA_NAME_SIZE < 20
#error METADATA_NAME_SIZE must be at least 20
#endif
	char name[METADATA_NAME_SIZE];
	char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
	char *fh_str;
	varray v;
	unsigned int i;
	unsigned int len;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (vol->local_path.str == NULL)
		zfsd_abort();
#endif

	len = (file_name->len <= METADATA_NAME_SIZE - (2 * 8 + 2)
		   ? file_name->len : METADATA_NAME_SIZE - (2 * 8 + 2));
	memcpy(name, file_name->str, len);
	name[len++] = '.';
	fh_str = name + len;
	sprintf(fh_str, "%08X%08X", fh->dev, fh->ino);
	len += 16;
#ifdef ENABLE_CHECKING
	if (name[len] != 0)
		zfsd_abort();
#endif

	for (i = 0; i < get_metadata_tree_depth(); i++)
	{
		tree[2 * i] = fh_str[15 - i];
		tree[2 * i + 1] = '/';
	}
	tree[2 * get_metadata_tree_depth()] = 0;

	varray_create(&v, sizeof(string), 4);
	VARRAY_USED(v) = 4;
	VARRAY_ACCESS(v, 0, string) = vol->local_path;
	VARRAY_ACCESS(v, 1, string).str = "/.shadow/";
	VARRAY_ACCESS(v, 1, string).len = 9;
	VARRAY_ACCESS(v, 2, string).str = tree;
	VARRAY_ACCESS(v, 2, string).len = 2 * get_metadata_tree_depth();
	VARRAY_ACCESS(v, 3, string).str = name;
	VARRAY_ACCESS(v, 3, string).len = len;

	xstringconcat_varray(path, &v);
	varray_destroy(&v);
}

/*! Create a full path to file FILE with access rights MODE. Return true if
   path exists at the end of this function. If VOL is not NULL we are creating 
   a shadow tree on volume VOL so insert metadata for new directories.  */

static bool create_path_for_file(string * file, unsigned int mode, volume vol)
{
	struct stat parent_st;
	struct stat st;
	char *last;
	char *end;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (file->len == 0)
		zfsd_abort();
	if (vol)
	{
		CHECK_MUTEX_LOCKED(&vol->mutex);
	}
#endif

	for (last = file->str + file->len - 1; last != file->str && *last != '/';
		 last--)
		;
	if (last == file->str)
		RETURN_BOOL(false);

	*last = 0;

	/* Find the first existing directory.  */
	for (end = last;;)
	{
		if (lstat(file->str, &parent_st) == 0)
		{
			if ((parent_st.st_mode & S_IFMT) != S_IFDIR)
				RETURN_BOOL(false);

			break;
		}

		for (; end != file->str && *end != '/'; end--)
			;
		if (end == file->str)
			RETURN_BOOL(false);

		*end = 0;
	}

	/* Create the path.  */
	for (;;)
	{
		if (end < last)
		{
			*end = '/';

			if (mkdir(file->str, mode) != 0)
				RETURN_BOOL(false);

			if (vol)
			{
				hardlink_list hl;
				metadata meta;
				string name;
				zfs_fh fh;

				if (lstat(file->str, &st) != 0
					|| (st.st_mode & S_IFMT) != S_IFDIR)
					RETURN_BOOL(false);

				fh.dev = st.st_dev;
				fh.ino = st.st_ino;
				meta.flags = METADATA_SHADOW_TREE;
				meta.modetype = GET_MODETYPE(GET_MODE(st.st_mode),
											 zfs_mode_to_ftype(st.st_mode));
				meta.uid = map_uid_node2zfs(st.st_uid);
				meta.gid = map_gid_node2zfs(st.st_gid);
				if (!lookup_metadata(vol, &fh, &meta, true))
				{
					MARK_VOLUME_DELETE(vol);
					RETURN_BOOL(false);
				}

				name.str = end + 1;
				name.len = strlen(end + 1);
				hl = hardlink_list_create(1, NULL);
				hardlink_list_insert(hl, parent_st.st_dev, parent_st.st_ino,
									 &name, true);
				if (!write_hardlinks(vol, &fh, &meta, hl))
					RETURN_BOOL(false);

				parent_st = st;
			}

			for (end++; end < last && *end; end++)
				;
		}
		if (end >= last)
		{
			*last = '/';
			RETURN_BOOL(true);
		}
	}

	RETURN_BOOL(false);
}

/*! Remove file FILE and its path upto depth TREE_DEPTH if it is empty.  */

static bool remove_file_and_path(string * file, unsigned int tree_depth)
{
	char *end;

	TRACE("");

	if (unlink(file->str) < 0 && errno != ENOENT)
		RETURN_BOOL(false);

	end = file->str + file->len;
	for (; tree_depth > 0; tree_depth--)
	{
		while (*end != '/')
			end--;
		*end = 0;

		if (rmdir(file->str) < 0)
		{
			if (errno == ENOENT || errno == ENOTEMPTY)
				RETURN_BOOL(true);
			RETURN_BOOL(false);
		}
	}

	RETURN_BOOL(true);
}

/*! Is the hash file HFILE opened? */

static bool hashfile_opened_p(hfile_t hfile)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(hfile->mutex);

	if (hfile->fd < 0)
		RETURN_BOOL(false);

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[hfile->fd].mutex);
	if (hfile->generation != metadata_fd_data[hfile->fd].generation)
	{
		zfsd_mutex_unlock(&metadata_fd_data[hfile->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		RETURN_BOOL(false);
	}

	metadata_fd_data[hfile->fd].heap_node
		=
		fibheap_replace_key(metadata_heap,
							metadata_fd_data[hfile->fd].heap_node,
							(fibheapkey_t) time(NULL));
	zfsd_mutex_unlock(&metadata_mutex);
	RETURN_BOOL(true);
}

/*! Is the interval file for interval tree TREE opened? */

static bool interval_opened_p(interval_tree tree)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(tree->mutex);

	if (tree->fd < 0)
		RETURN_BOOL(false);

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[tree->fd].mutex);
	if (tree->generation != metadata_fd_data[tree->fd].generation)
	{
		zfsd_mutex_unlock(&metadata_fd_data[tree->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		RETURN_BOOL(false);
	}

	metadata_fd_data[tree->fd].heap_node
		=
		fibheap_replace_key(metadata_heap,
							metadata_fd_data[tree->fd].heap_node,
							(fibheapkey_t) time(NULL));
	zfsd_mutex_unlock(&metadata_mutex);
	RETURN_BOOL(true);
}

/*! Is the file for journal JOURNAL opened? */

static bool journal_opened_p(journal_t journal)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(journal->mutex);

	if (journal->fd < 0)
		RETURN_BOOL(false);

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[journal->fd].mutex);
	if (journal->generation != metadata_fd_data[journal->fd].generation)
	{
		zfsd_mutex_unlock(&metadata_fd_data[journal->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		RETURN_BOOL(false);
	}

	metadata_fd_data[journal->fd].heap_node
		= fibheap_replace_key(metadata_heap,
							  metadata_fd_data[journal->fd].heap_node,
							  (fibheapkey_t) time(NULL));
	zfsd_mutex_unlock(&metadata_mutex);
	RETURN_BOOL(true);
}

/*! Initialize file descriptor for hash file HFILE.  */

static void init_hashfile_fd(hfile_t hfile)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (hfile->fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(hfile->mutex);
	CHECK_MUTEX_LOCKED(&metadata_mutex);
	CHECK_MUTEX_LOCKED(&metadata_fd_data[hfile->fd].mutex);

	metadata_fd_data[hfile->fd].fd = hfile->fd;
	metadata_fd_data[hfile->fd].generation++;
	hfile->generation = metadata_fd_data[hfile->fd].generation;
	metadata_fd_data[hfile->fd].heap_node
		= fibheap_insert(metadata_heap, (fibheapkey_t) time(NULL),
						 &metadata_fd_data[hfile->fd]);
}

/*! Initialize file descriptor for interval tree TREE.  */

static void init_interval_fd(interval_tree tree)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (tree->fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(tree->mutex);
	CHECK_MUTEX_LOCKED(&metadata_mutex);
	CHECK_MUTEX_LOCKED(&metadata_fd_data[tree->fd].mutex);

	metadata_fd_data[tree->fd].fd = tree->fd;
	metadata_fd_data[tree->fd].generation++;
	tree->generation = metadata_fd_data[tree->fd].generation;
	metadata_fd_data[tree->fd].heap_node
		= fibheap_insert(metadata_heap, (fibheapkey_t) time(NULL),
						 &metadata_fd_data[tree->fd]);
}

/*! Initialize file descriptor for journal JOURNAL.  */

static void init_journal_fd(journal_t journal)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (journal->fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(journal->mutex);
	CHECK_MUTEX_LOCKED(&metadata_mutex);
	CHECK_MUTEX_LOCKED(&metadata_fd_data[journal->fd].mutex);

	metadata_fd_data[journal->fd].fd = journal->fd;
	metadata_fd_data[journal->fd].generation++;
	journal->generation = metadata_fd_data[journal->fd].generation;
	metadata_fd_data[journal->fd].heap_node
		= fibheap_insert(metadata_heap, (fibheapkey_t) time(NULL),
						 &metadata_fd_data[journal->fd]);
}

/*! Close file descriptor FD of metadata file.  */

static void close_metadata_fd(int fd)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&metadata_mutex);
	CHECK_MUTEX_LOCKED(&metadata_fd_data[fd].mutex);

#ifdef ENABLE_CHECKING
	if (metadata_fd_data[fd].fd < 0)
		zfsd_abort();
#endif
	metadata_fd_data[fd].fd = -1;
	metadata_fd_data[fd].generation++;
	close(fd);
	if (metadata_fd_data[fd].heap_node)
	{
		fibheap_delete_node(metadata_heap, metadata_fd_data[fd].heap_node);
		metadata_fd_data[fd].heap_node = NULL;
	}
	zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
}

/*! Open metadata file PATHNAME with open flags FLAGS and mode MODE.  */

static int open_metadata(const char *pathname, int flags, mode_t mode)
{
	int fd;

	TRACE("");

  retry_open:
	fd = open(pathname, flags, mode);
	if ((fd < 0 && errno == EMFILE)
		|| (fd >= 0
			&& fibheap_size(metadata_heap) >= (unsigned int)max_metadata_fds))
	{
		metadata_fd_data_t *fd_data;

		zfsd_mutex_lock(&metadata_mutex);
		fd_data = (metadata_fd_data_t *) fibheap_extract_min(metadata_heap);
#ifdef ENABLE_CHECKING
		if (!fd_data && fibheap_size(metadata_heap) > 0)
			zfsd_abort();
#endif
		if (fd_data)
		{
			zfsd_mutex_lock(&fd_data->mutex);
			fd_data->heap_node = NULL;
			if (fd_data->fd >= 0)
				close_metadata_fd(fd_data->fd);
			else
				zfsd_mutex_unlock(&fd_data->mutex);
		}
		zfsd_mutex_unlock(&metadata_mutex);
		if (fd_data)
			goto retry_open;
	}

	RETURN_INT(fd);
}

/*! Open metadata file of type TYPE for file handle FH on volume VOL with
   path PATH, open flags FLAGS and mode MODE.  */

static int
open_fh_metadata(string * path, volume vol, zfs_fh * fh, metadata_type type,
				 int flags, mode_t mode)
{
	int fd;
	unsigned int i;
	struct stat st;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	fd = open_metadata(path->str, flags, mode);
	if (fd < 0)
	{
		if (errno != ENOENT)
			RETURN_INT(-1);

		if ((flags & O_ACCMODE) != O_RDONLY)
		{
			if (!create_path_for_file(path, S_IRWXU, NULL))
			{
				if (errno == ENOENT)
					errno = 0;
				RETURN_INT(-1);
			}

			for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
				if (i != get_metadata_tree_depth())
				{
					string old_path;

					build_fh_metadata_path(&old_path, vol, fh, type, i);
					if (rename(old_path.str, path->str) == 0)
					{
						free(old_path.str);
						break;
					}
					free(old_path.str);
				}
		}
		else
		{
			bool created = false;

			for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
				if (i != get_metadata_tree_depth())
				{
					string old_path;

					build_fh_metadata_path(&old_path, vol, fh, type, i);
					if (stat(old_path.str, &st) == 0
						&& (st.st_mode & S_IFMT) == S_IFREG)
					{
						if (!created)
						{
							if (!create_path_for_file(path, S_IRWXU, NULL))
							{
								if (errno == ENOENT)
									errno = 0;
								free(old_path.str);
								RETURN_INT(-1);
							}
							created = true;
						}

						if (rename(old_path.str, path->str) == 0)
						{
							free(old_path.str);
							break;
						}
					}
					free(old_path.str);
				}
		}

		fd = open_metadata(path->str, flags, mode);
	}

	RETURN_INT(fd);
}

/*! Open and initialize file descriptor for hash file HFILE of type TYPE.  */

static int open_hash_file(volume vol, metadata_type type)
{
	hfile_t hfile;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	switch (type)
	{
	case METADATA_TYPE_METADATA:
		hfile = vol->metadata;
		break;

	case METADATA_TYPE_FH_MAPPING:
		hfile = vol->fh_mapping;
		break;

	default:
		zfsd_abort();
	}

	fd = open_metadata(hfile->file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0)
		RETURN_INT(fd);

	hfile->fd = fd;

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[fd].mutex);
	init_hashfile_fd(hfile);
	zfsd_mutex_unlock(&metadata_mutex);

	RETURN_INT(fd);
}

/*! Open and initialize file descriptor for interval of type TYPE for file
   handle FH on volume VOL.  */

static int open_interval_file(volume vol, internal_fh fh, metadata_type type)
{
	interval_tree tree;
	string path;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	build_fh_metadata_path(&path, vol, &fh->local_fh, type,
						   get_metadata_tree_depth());
	fd = open_fh_metadata(&path, vol, &fh->local_fh, type,
						  O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	free(path.str);
	if (fd < 0)
		RETURN_INT(fd);

	if (lseek(fd, 0, SEEK_END) == (off_t) - 1)
	{
		message(LOG_ERROR, FACILITY_DATA, "lseek: %s\n", strerror(errno));
		close(fd);
		RETURN_INT(-1);
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
		zfsd_abort();
	}

	CHECK_MUTEX_LOCKED(tree->mutex);

	tree->fd = fd;

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[fd].mutex);
	init_interval_fd(tree);
	zfsd_mutex_unlock(&metadata_mutex);

	RETURN_INT(fd);
}

/*! Open and initialize file descriptor for journal JOURNAL for file handle
   FH on volume VOL.  */

static int open_journal_file(volume vol, journal_t journal, zfs_fh * fh)
{
	string path;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(journal->mutex);

	build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_JOURNAL,
						   get_metadata_tree_depth());
	fd = open_fh_metadata(&path, vol, fh, METADATA_TYPE_JOURNAL,
						  O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	free(path.str);
	if (fd < 0)
		RETURN_INT(fd);

	if (lseek(fd, 0, SEEK_END) == (off_t) - 1)
	{
		message(LOG_ERROR, FACILITY_DATA, "lseek: %s\n", strerror(errno));
		close(fd);
		RETURN_INT(-1);
	}

	journal->fd = fd;

	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[fd].mutex);
	init_journal_fd(journal);
	zfsd_mutex_unlock(&metadata_mutex);

	RETURN_INT(fd);
}

/*! Delete interval file PATH for interval tree TREE of type TYPE for
   internal file handle FH on volume VOL. Return true if it was useless.  */

static bool
delete_useless_interval_file(volume vol, internal_fh fh, metadata_type type,
							 interval_tree tree, string * path)
{
	TRACE("");

	switch (type)
	{
	case METADATA_TYPE_UPDATED:
		if (tree->size == 1
			&& INTERVAL_START(tree->splay->root) == 0
			&& INTERVAL_END(tree->splay->root) == fh->attr.size)
		{
			if (!set_metadata_flags(vol, fh,
									fh->meta.flags & ~METADATA_UPDATED_TREE))
				MARK_VOLUME_DELETE(vol);

			if (!remove_file_and_path(path, get_metadata_tree_depth()))
				MARK_VOLUME_DELETE(vol);

			RETURN_BOOL(true);
		}
		else
		{
			if (!set_metadata_flags(vol, fh,
									fh->meta.flags | METADATA_UPDATED_TREE))
				MARK_VOLUME_DELETE(vol);
		}
		break;

	case METADATA_TYPE_MODIFIED:
		if (tree->size == 0)
		{
			if (!set_metadata_flags(vol, fh,
									fh->meta.flags & ~METADATA_MODIFIED_TREE))
				MARK_VOLUME_DELETE(vol);

			if (!remove_file_and_path(path, get_metadata_tree_depth()))
				MARK_VOLUME_DELETE(vol);

			RETURN_BOOL(true);
		}
		else
		{
			if (!set_metadata_flags(vol, fh,
									fh->meta.flags | METADATA_MODIFIED_TREE))
				MARK_VOLUME_DELETE(vol);
		}
		break;

	default:
		zfsd_abort();
	}

	RETURN_BOOL(false);
}

/*! Flush interval tree of type TYPE for file handle FH on volume VOL to file 
   PATH.  */

static bool
flush_interval_tree_1(volume vol, internal_fh fh, metadata_type type,
					  string * path)
{
	interval_tree tree;
	string new_path;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	switch (type)
	{
	case METADATA_TYPE_UPDATED:
		tree = fh->updated;
		break;

	case METADATA_TYPE_MODIFIED:
		tree = fh->modified;
		break;

	default:
		zfsd_abort();
	}

	CHECK_MUTEX_LOCKED(tree->mutex);

	close_interval_file(tree);

	if (delete_useless_interval_file(vol, fh, type, tree, path))
	{
		tree->deleted = false;
		free(path->str);
		RETURN_BOOL(true);
	}

	append_string(&new_path, path, ".new", 4);
	fd = open_fh_metadata(&new_path, vol, &fh->local_fh, type,
						  O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

	if (fd < 0)
	{
		free(new_path.str);
		free(path->str);
		RETURN_BOOL(false);
	}

	if (!interval_tree_write(tree, fd))
	{
		close(fd);
		remove_file_and_path(&new_path, get_metadata_tree_depth());
		free(new_path.str);
		free(path->str);
		RETURN_BOOL(false);
	}

	rename(new_path.str, path->str);
	tree->deleted = false;

#ifdef ENABLE_CHECKING
	if (tree->fd >= 0)
		zfsd_abort();
#endif
	tree->fd = fd;
	zfsd_mutex_lock(&metadata_mutex);
	zfsd_mutex_lock(&metadata_fd_data[tree->fd].mutex);
	init_interval_fd(tree);
	zfsd_mutex_unlock(&metadata_fd_data[tree->fd].mutex);
	zfsd_mutex_unlock(&metadata_mutex);

	free(new_path.str);
	free(path->str);
	RETURN_BOOL(true);
}

/*! return stat's mode from file type */
uint32_t zfs_ftype_to_mode(ftype type)
{
	switch (type)
	{
	case FT_BAD:
		return 0;
	case FT_REG:
		return S_IFREG;
	case FT_DIR:
		return S_IFDIR;
	case FT_LNK:
		return S_IFLNK;
	case FT_BLK:
		return S_IFBLK;
	case FT_CHR:
		return S_IFCHR;
	case FT_SOCK:
		return S_IFSOCK;
	case FT_FIFO:
		return S_IFIFO;
	case FT_LAST_AND_UNUSED:
	default:
		return 0;
	}
}

/*! Return file type from struct stat's MODE.  */

ftype zfs_mode_to_ftype(uint32_t mode)
{
	switch (mode & S_IFMT)
	{
	case S_IFSOCK:
		return FT_SOCK;

	case S_IFLNK:
		return FT_LNK;

	case S_IFREG:
		return FT_REG;

	case S_IFBLK:
		return FT_BLK;

	case S_IFDIR:
		return FT_DIR;

	case S_IFCHR:
		return FT_CHR;

	case S_IFIFO:
		return FT_FIFO;

	default:
		return FT_BAD;
	}

	return FT_BAD;
}

/*! Initialize hash file containing metadata for volume VOL.  */

bool init_volume_metadata(volume vol)
{
	hashfile_header header;
	int fd;
	string path;
	struct stat st;
	bool insert_volume_root;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (vol->local_path.str == NULL)
		zfsd_abort();
#endif

	build_metadata_path(&path, vol, METADATA_TYPE_METADATA);
	vol->metadata = hfile_create(sizeof(metadata),
								 offsetof(metadata, parent_dev), 32,
								 metadata_hash, metadata_eq, metadata_decode,
								 metadata_encode, path.str, &vol->mutex);
	insert_volume_root = (lstat(vol->local_path.str, &st) < 0);

	if (!create_path_for_file(&path, S_IRWXU, NULL))
	{
		free(path.str);
		close_volume_metadata(vol);
		RETURN_BOOL(false);
	}
	free(path.str);

	fd = open_hash_file(vol, METADATA_TYPE_METADATA);
	if (fd < 0)
	{
		close_volume_metadata(vol);
		RETURN_BOOL(false);
	}

	if (fstat(fd, &st) < 0)
	{
		message(LOG_WARNING, FACILITY_DATA, "%s: fstat: %s\n",
				vol->metadata->file_name, strerror(errno));
		zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
		close_volume_metadata(vol);
		RETURN_BOOL(false);
	}

	if (!hfile_init(vol->metadata, &st))
	{
		if ((st.st_mode & S_IFMT) != S_IFREG)
		{
			message(LOG_ERROR, FACILITY_DATA, "%s: Not a regular file\n",
					vol->metadata->file_name);
			zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
			close_volume_metadata(vol);
			RETURN_BOOL(false);
		}
		else if ((uint64_t) st.st_size < sizeof(metadata))
		{
			header.n_elements = 0;
			header.n_deleted = 0;
			if (!full_write(fd, &header, sizeof(header)))
			{
				zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
				unlink(vol->metadata->file_name);
				close_volume_metadata(vol);
				RETURN_BOOL(false);
			}

			if (ftruncate(fd, ((uint64_t) vol->metadata->size
							   * sizeof(metadata) + sizeof(metadata))) < 0)
			{
				zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
				unlink(vol->metadata->file_name);
				close_volume_metadata(vol);
				RETURN_BOOL(false);
			}
		}
		else
		{
			zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
			close_volume_metadata(vol);
			RETURN_BOOL(false);
		}
	}

	zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);

	if (insert_volume_root)
	{
		if (!init_metadata_for_created_volume_root(vol))
		{
			close_volume_metadata(vol);
			RETURN_BOOL(false);
		}
	}

	build_metadata_path(&path, vol, METADATA_TYPE_FH_MAPPING);
	vol->fh_mapping = hfile_create(sizeof(fh_mapping), sizeof(fh_mapping),
								   32, fh_mapping_hash,
								   fh_mapping_eq, fh_mapping_decode,
								   fh_mapping_encode, path.str, &vol->mutex);
	free(path.str);

	fd = open_hash_file(vol, METADATA_TYPE_FH_MAPPING);
	if (fd < 0)
	{
		close_volume_metadata(vol);
		RETURN_BOOL(false);
	}

	if (fstat(fd, &st) < 0)
	{
		message(LOG_WARNING, FACILITY_DATA, "%s: fstat: %s\n",
				vol->fh_mapping->file_name, strerror(errno));
		zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
		close_volume_metadata(vol);
		RETURN_BOOL(false);
	}

	if (!hfile_init(vol->fh_mapping, &st))
	{
		if ((st.st_mode & S_IFMT) != S_IFREG)
		{
			message(LOG_WARNING, FACILITY_DATA, "%s: Not a regular file\n",
					vol->fh_mapping->file_name);
			zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
			close_volume_metadata(vol);
			RETURN_BOOL(false);
		}
		else if ((uint64_t) st.st_size < sizeof(fh_mapping))
		{
			header.n_elements = 0;
			header.n_deleted = 0;
			if (!full_write(fd, &header, sizeof(header)))
			{
				zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
				unlink(vol->fh_mapping->file_name);
				close_volume_metadata(vol);
				RETURN_BOOL(false);
			}

			if (ftruncate(fd, ((uint64_t) vol->fh_mapping->size
							   * sizeof(fh_mapping) + sizeof(fh_mapping))) < 0)
			{
				zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
				unlink(vol->fh_mapping->file_name);
				close_volume_metadata(vol);
				RETURN_BOOL(false);
			}
		}
		else
		{
			zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);
			close_volume_metadata(vol);
			RETURN_BOOL(false);
		}
	}

	zfsd_mutex_unlock(&metadata_fd_data[fd].mutex);

	RETURN_BOOL(true);
}

/*! Close file for hahs file HFILE.  */

static void close_hash_file(hfile_t hfile)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(hfile->mutex);

	if (hfile->fd >= 0)
	{
		zfsd_mutex_lock(&metadata_mutex);
		zfsd_mutex_lock(&metadata_fd_data[hfile->fd].mutex);
		if (hfile->generation == metadata_fd_data[hfile->fd].generation)
			close_metadata_fd(hfile->fd);
		else
			zfsd_mutex_unlock(&metadata_fd_data[hfile->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		hfile->fd = -1;
	}

	RETURN_VOID;
}

/*! Close hash file containing metadata for volume VOL.  */

void close_volume_metadata(volume vol)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (vol->metadata)
	{
		close_hash_file(vol->metadata);
		hfile_destroy(vol->metadata);
		vol->metadata = NULL;
	}
	if (vol->fh_mapping)
	{
		close_hash_file(vol->fh_mapping);
		hfile_destroy(vol->fh_mapping);
		vol->fh_mapping = NULL;
	}
	MARK_VOLUME_DELETE(vol);
}

/*! Close file for interval tree TREE.  */

void close_interval_file(interval_tree tree)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(tree->mutex);

	if (tree->fd >= 0)
	{
		zfsd_mutex_lock(&metadata_mutex);
		zfsd_mutex_lock(&metadata_fd_data[tree->fd].mutex);
		if (tree->generation == metadata_fd_data[tree->fd].generation)
			close_metadata_fd(tree->fd);
		else
			zfsd_mutex_unlock(&metadata_fd_data[tree->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		tree->fd = -1;
	}
}

/*! Close file for journal JOURNAL.  */

void close_journal_file(journal_t journal)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(journal->mutex);

	if (journal->fd >= 0)
	{
		zfsd_mutex_lock(&metadata_mutex);
		zfsd_mutex_lock(&metadata_fd_data[journal->fd].mutex);
		if (journal->generation == metadata_fd_data[journal->fd].generation)
			close_metadata_fd(journal->fd);
		else
			zfsd_mutex_unlock(&metadata_fd_data[journal->fd].mutex);
		zfsd_mutex_unlock(&metadata_mutex);
		journal->fd = -1;
	}
}

/*! Initialize interval tree of type TYPE for file handle FH on volume VOL.  */

static bool init_interval_tree(volume vol, internal_fh fh, metadata_type type)
{
	int fd;
	string path;
	struct stat st;
	interval_tree *treep;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	switch (type)
	{
	case METADATA_TYPE_UPDATED:
		if (!(fh->meta.flags & METADATA_UPDATED_TREE))
		{
			fh->updated = interval_tree_create(62, &fh->mutex);
			interval_tree_insert(fh->updated, 0, fh->attr.size);
			RETURN_BOOL(true);
		}
		treep = &fh->updated;
		break;

	case METADATA_TYPE_MODIFIED:
		if (!(fh->meta.flags & METADATA_MODIFIED_TREE))
		{
			fh->modified = interval_tree_create(62, &fh->mutex);
			RETURN_BOOL(true);
		}
		treep = &fh->modified;
		break;

	default:
		zfsd_abort();
	}

	build_fh_metadata_path(&path, vol, &fh->local_fh, type,
						   get_metadata_tree_depth());
	fd = open_fh_metadata(&path, vol, &fh->local_fh, type, O_RDONLY, 0);
	if (fd < 0)
	{
		if (errno != ENOENT)
		{
			free(path.str);
			RETURN_BOOL(false);
		}

		*treep = interval_tree_create(62, &fh->mutex);
	}
	else
	{
		if (fstat(fd, &st) < 0)
		{
			message(LOG_WARNING, FACILITY_DATA, "%s: fstat: %s\n", path.str,
					strerror(errno));
			close(fd);
			free(path.str);
			RETURN_BOOL(false);
		}

		if ((st.st_mode & S_IFMT) != S_IFREG)
		{
			message(LOG_WARNING, FACILITY_DATA, "%s: Not a regular file\n",
					path.str);
			close(fd);
			free(path.str);
			RETURN_BOOL(false);
		}

		if (st.st_size % sizeof(interval) != 0)
		{
			message(LOG_WARNING, FACILITY_DATA,
					"%s: Interval list is not aligned\n", path.str);
			close(fd);
			free(path.str);
			RETURN_BOOL(false);
		}

		*treep = interval_tree_create(62, &fh->mutex);
		if (!interval_tree_read(*treep, fd, st.st_size / sizeof(interval)))
		{
			interval_tree_destroy(*treep);
			*treep = NULL;
			close(fd);
			free(path.str);
			RETURN_BOOL(false);
		}

		close(fd);
	}

	switch (type)
	{
	case METADATA_TYPE_UPDATED:
	case METADATA_TYPE_MODIFIED:
		interval_tree_delete(*treep, fh->attr.size, UINT64_MAX);
		break;

	default:
		zfsd_abort();
	}

	RETURN_BOOL(flush_interval_tree_1(vol, fh, type, &path));
}

/*! Flush the interval tree of type TYPE for file handle FH on volume VOL to
   file.  */

bool flush_interval_tree(volume vol, internal_fh fh, metadata_type type)
{
	string path;

	TRACE("");

	build_fh_metadata_path(&path, vol, &fh->local_fh, type,
						   get_metadata_tree_depth());

	RETURN_BOOL(flush_interval_tree_1(vol, fh, type, &path));
}

/*! Flush the interval tree of type TYPE for file handle FH on volume VOL to
   file and free the interval tree.  */

static bool free_interval_tree(volume vol, internal_fh fh, metadata_type type)
{
	string path;
	interval_tree tree, *treep;
	bool r;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

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
		zfsd_abort();
	}

	CHECK_MUTEX_LOCKED(tree->mutex);

	build_fh_metadata_path(&path, vol, &fh->local_fh, type,
						   get_metadata_tree_depth());

	r = flush_interval_tree_1(vol, fh, type, &path);
	close_interval_file(tree);
	interval_tree_destroy(tree);
	*treep = NULL;

	RETURN_BOOL(r);
}

/*! Write the interval [START, END) to the end of interval file of type TYPE
   for file handle FH on volume VOL.  Open the interval file for appending
   when it is not opened.  */

bool
append_interval(volume vol, internal_fh fh, metadata_type type,
				uint64_t start, uint64_t end)
{
	interval_tree tree;
	interval i;
	string path;
	bool r;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	switch (type)
	{
	case METADATA_TYPE_UPDATED:
		tree = fh->updated;
		break;

	case METADATA_TYPE_MODIFIED:
		tree = fh->modified;
		break;

	default:
		zfsd_abort();
	}

	CHECK_MUTEX_LOCKED(tree->mutex);
	interval_tree_insert(tree, start, end);

	if (!interval_opened_p(tree))
	{
		if (open_interval_file(vol, fh, type) < 0)
			RETURN_BOOL(false);
	}
	else
	{
		if (lseek(tree->fd, 0, SEEK_END) == (off_t) - 1)
		{
			message(LOG_WARNING, FACILITY_DATA, "lseek: %s\n",
					strerror(errno));
			zfsd_mutex_unlock(&metadata_fd_data[tree->fd].mutex);
			RETURN_BOOL(false);
		}
	}

	i.start = u64_to_le(start);
	i.end = u64_to_le(end);
	r = full_write(tree->fd, &i, sizeof(interval));

	zfsd_mutex_unlock(&metadata_fd_data[tree->fd].mutex);

	build_fh_metadata_path(&path, vol, &fh->local_fh, type,
						   get_metadata_tree_depth());
	delete_useless_interval_file(vol, fh, type, tree, &path);
	free(path.str);

	RETURN_BOOL(r);
}

/*! Set version in attributes ATTR according to metadata META.  */

void set_attr_version(fattr * attr, metadata * meta)
{
	TRACE("");

	attr->version = meta->local_version;
}

/*! Init metadata for root of volume VOL according to ST so that volume root
   would be updated.  */

static bool init_metadata_for_created_volume_root(volume vol)
{
	struct stat st;
	metadata meta;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (lstat(vol->local_path.str, &st) < 0)
		RETURN_BOOL(false);

	if ((st.st_mode & S_IFMT) != S_IFDIR)
		RETURN_BOOL(false);

	if (!hashfile_opened_p(vol->metadata))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_METADATA);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	meta.dev = st.st_dev;
	meta.ino = st.st_ino;
	if (!hfile_lookup(vol->metadata, &meta))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
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
		zfs_fh_undefine(meta.master_fh);
		meta.modetype = GET_MODETYPE(GET_MODE(st.st_mode),
									 zfs_mode_to_ftype(st.st_mode));
		meta.uid = map_uid_node2zfs(st.st_uid);
		meta.gid = map_gid_node2zfs(st.st_gid);
		meta.parent_dev = (uint32_t) - 1;
		meta.parent_ino = (uint32_t) - 1;
		memset(meta.name, 0, METADATA_NAME_SIZE);

		if (!hfile_insert(vol->metadata, &meta, false))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(false);
		}
	}

	zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
	RETURN_BOOL(true);
}

/*! Lookup metadata for file handle FH on volume VOL.  Store the metadata to
   META and update FH->GEN.  Insert the metadata to hash file if INSERT is
   true and the metadata was not found.  */

bool lookup_metadata(volume vol, zfs_fh * fh, metadata * meta, bool insert)
{
	uint32_t flags = meta->flags;
	uint32_t modetype = meta->modetype;
	uint32_t uid = meta->uid;
	uint32_t gid = meta->gid;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!vol->metadata)
		zfsd_abort();
	if (!vol->local_path.str)
		zfsd_abort();
#endif

	if (!hashfile_opened_p(vol->metadata))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_METADATA);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	meta->dev = fh->dev;
	meta->ino = fh->ino;
	if (!hfile_lookup(vol->metadata, meta))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
	}

	if (meta->slot_status == VALID_SLOT
		&& GET_MODETYPE_TYPE(meta->modetype) == FT_BAD)
	{
		/* Preserve MODETYPE, UID and GID.  */
		meta->modetype = modetype;
		meta->uid = uid;
		meta->gid = gid;

		if (insert)
		{
			meta->flags = flags;
			zfs_fh_undefine(meta->master_fh);
			if (!hfile_insert(vol->metadata, meta, false))
			{
				zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
				RETURN_BOOL(false);
			}
		}
	}
	else if (insert && meta->slot_status != VALID_SLOT)
	{
		meta->slot_status = VALID_SLOT;
		meta->flags = flags;
		meta->dev = fh->dev;
		meta->ino = fh->ino;
		meta->gen = 1;
		meta->local_version = 1;
		meta->master_version = vol->is_copy ? 0 : 1;
		zfs_fh_undefine(meta->master_fh);
		meta->modetype = modetype;
		meta->uid = uid;
		meta->gid = gid;
		meta->parent_dev = (uint32_t) - 1;
		meta->parent_ino = (uint32_t) - 1;
		memset(meta->name, 0, METADATA_NAME_SIZE);

		if (!hfile_insert(vol->metadata, meta, false))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(false);
		}
	}
	fh->gen = meta->gen;
	zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);

	if (meta->slot_status == VALID_SLOT
		&& GET_MODETYPE_TYPE(modetype) != GET_MODETYPE_TYPE(meta->modetype)
		&& GET_MODETYPE_TYPE(modetype) != FT_BAD)
	{
		meta->modetype = modetype;
		meta->uid = uid;
		meta->gid = gid;
		RETURN_BOOL(delete_metadata_of_created_file(vol, fh, meta));
	}
	RETURN_BOOL(true);
}

/*! Get metadata for file handle FH on volume VOL. Store the metadata to META 
   and update FH->GEN.  Unlock the volume.  */

bool get_metadata(volume vol, zfs_fh * fh, metadata * meta)
{
	TRACE("");

	if (!vol)
		RETURN_BOOL(false);

	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!meta)
		zfsd_abort();
#endif

	if (!lookup_metadata(vol, fh, meta, true))
	{
		MARK_VOLUME_DELETE(vol);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_BOOL(false);
	}

	zfsd_mutex_unlock(&vol->mutex);
	RETURN_BOOL(true);
}

/*! Delete file handle mapping MAP on volume VOL.  */

static bool delete_fh_mapping(volume vol, fh_mapping * map)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!hashfile_opened_p(vol->fh_mapping))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_FH_MAPPING);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	if (!hfile_delete(vol->fh_mapping, map))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
		RETURN_BOOL(false);
	}

	zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
	RETURN_BOOL(true);
}

/*! Get file handle mapping for master file handle MASTER_FH on volume VOL
   and store it to MAP.  */

bool
get_fh_mapping_for_master_fh(volume vol, zfs_fh * master_fh, fh_mapping * map)
{
	metadata meta;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!hashfile_opened_p(vol->fh_mapping))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_FH_MAPPING);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	map->master_fh.dev = master_fh->dev;
	map->master_fh.ino = master_fh->ino;
	if (!hfile_lookup(vol->fh_mapping, map))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
		RETURN_BOOL(false);
	}

	if (map->slot_status == VALID_SLOT && map->master_fh.gen < master_fh->gen)
	{
		/* There is a master file handle with older generation in the hash
		   file so delete it and return undefined local file handle.  */
		if (!hfile_delete(vol->fh_mapping, map))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
			RETURN_BOOL(false);
		}
		map->slot_status = DELETED_SLOT;
	}
	zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);

	if (map->slot_status == VALID_SLOT)
	{
		/* Check whether the local file handle is still valid.  */
		if (!hashfile_opened_p(vol->metadata))
		{
			int fd;

			fd = open_hash_file(vol, METADATA_TYPE_METADATA);
			if (fd < 0)
				RETURN_BOOL(false);
		}

		meta.dev = map->local_fh.dev;
		meta.ino = map->local_fh.ino;
		if (!hfile_lookup(vol->metadata, &meta))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(false);
		}
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);

		/* If local file is not valid delete the mapping.  */
		if (meta.slot_status != VALID_SLOT || meta.gen != map->local_fh.gen)
		{
			if (!delete_fh_mapping(vol, map))
				RETURN_BOOL(false);
			map->slot_status = DELETED_SLOT;
		}
	}

	RETURN_BOOL(true);
}

/*! Write the metadata META to list file on volume VOL. Return false on file
   error.  */

bool flush_metadata(volume vol, metadata * meta)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!hashfile_opened_p(vol->metadata))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_METADATA);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	if (!hfile_insert(vol->metadata, meta, true))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
	}

	zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
	RETURN_BOOL(true);
}

/*! Set metadata (FLAGS, LOCAL_VERSION, MASTER_VERSION) for file handle FH on 
   volume VOL.  Return false on file error.  */

bool
set_metadata(volume vol, internal_fh fh, uint32_t flags,
			 uint64_t local_version, uint64_t master_version)
{
	bool modified;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

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
	if (vol->is_copy)
	{
		if (fh->meta.master_version != master_version)
		{
			fh->meta.master_version = master_version;
			modified = true;
		}
	}
	else
	{
		fh->meta.master_version = local_version;
	}

	if (!modified)
		RETURN_BOOL(true);

	set_attr_version(&fh->attr, &fh->meta);

	RETURN_BOOL(flush_metadata(vol, &fh->meta));
}

/*! Set metadata flags FLAGS for file handle FH on volume VOL. Return false
   on file error.  */

bool set_metadata_flags(volume vol, internal_fh fh, uint32_t flags)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	if (fh->meta.flags == flags)
		RETURN_BOOL(true);

	fh->meta.flags = flags;

	RETURN_BOOL(flush_metadata(vol, &fh->meta));
}

/*! Set master_fh to MASTER_FH in metadata for file handle FH on volume VOL
   and update reverse file handle mapping.  */

bool set_metadata_master_fh(volume vol, internal_fh fh, zfs_fh * master_fh)
{
	fh_mapping map;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	if (ZFS_FH_EQ(fh->meta.master_fh, *master_fh))
		RETURN_BOOL(true);

	if (!hashfile_opened_p(vol->fh_mapping))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_FH_MAPPING);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	if (fh->meta.master_fh.dev == master_fh->dev
		&& fh->meta.master_fh.ino == master_fh->ino)
	{
		map.slot_status = VALID_SLOT;
		map.master_fh = *master_fh;
		map.local_fh = fh->local_fh;
		if (!hfile_insert(vol->fh_mapping, &map, false))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
			RETURN_BOOL(false);
		}
	}
	else
	{
		/* Delete original reverse file handle mapping.  */
		map.master_fh.dev = fh->meta.master_fh.dev;
		map.master_fh.ino = fh->meta.master_fh.ino;
		if (!hfile_delete(vol->fh_mapping, &map))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
			RETURN_BOOL(false);
		}

		/* Set new reverse file handle mapping.  */
		if (!zfs_fh_undefined(*master_fh))
		{
			map.slot_status = VALID_SLOT;
			map.master_fh = *master_fh;
			map.local_fh = fh->local_fh;
			if (!hfile_insert(vol->fh_mapping, &map, false))
			{
				zfsd_mutex_unlock(&metadata_fd_data
								  [vol->fh_mapping->fd].mutex);
				RETURN_BOOL(false);
			}
		}
	}
	zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);

	fh->meta.master_fh = *master_fh;
	RETURN_BOOL(flush_metadata(vol, &fh->meta));
}

/*! Increase the local version for file FH on volume VOL. Return false on
   file error.  */

bool inc_local_version(volume vol, internal_fh fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	fh->meta.local_version++;
	if (!vol->is_copy)
		fh->meta.master_version = fh->meta.local_version;
	set_attr_version(&fh->attr, &fh->meta);

	RETURN_BOOL(flush_metadata(vol, &fh->meta));
}

/*! Increase the local version for file FH on volume VOL and set MODIFIED
   flag. Return false on file error.  */

bool inc_local_version_and_modified(volume vol, internal_fh fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	fh->meta.local_version++;
	if (!vol->is_copy)
		fh->meta.master_version = fh->meta.local_version;
	fh->meta.flags |= METADATA_MODIFIED_TREE;
	set_attr_version(&fh->attr, &fh->meta);

	RETURN_BOOL(flush_metadata(vol, &fh->meta));
}

/*! Delete all metadata files for file on volume VOL with device DEV and
   inode INO and hardlink [PARENT_DEV, PARENT_INO, NAME].  */

bool
delete_metadata(volume vol, metadata * meta, uint32_t dev, uint32_t ino,
				uint32_t parent_dev, uint32_t parent_ino, string * name)
{
	fh_mapping map;
	zfs_fh fh;
	string path;
	int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	fh.dev = dev;
	fh.ino = ino;

	/* Delete hardlink.  */
	if (name && name->str)
	{
		hardlink_list hl;
		int fd;

		build_fh_metadata_path(&path, vol, &fh, METADATA_TYPE_HARDLINKS,
							   get_metadata_tree_depth());
		fd = open_fh_metadata(&path, vol, &fh, METADATA_TYPE_HARDLINKS,
							  O_RDONLY, S_IRUSR | S_IWUSR);
		free(path.str);
		if (fd >= 0)
		{
			hl = hardlink_list_create(2, NULL);
			read_hardlinks_file(hl, fd);

			hardlink_list_delete(hl, parent_dev, parent_ino, name);
			if (hl->first)
				RETURN_BOOL(write_hardlinks(vol, &fh, meta, hl));
			else
			{
				hardlink_list_destroy(hl);
				delete_hardlinks_file(vol, &fh);
			}
		}
	}

	/* Delete interval files.  */
	for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
	{
		build_fh_metadata_path(&path, vol, &fh, METADATA_TYPE_UPDATED, i);
		if (!remove_file_and_path(&path, i))
			MARK_VOLUME_DELETE(vol);
		free(path.str);
		build_fh_metadata_path(&path, vol, &fh, METADATA_TYPE_MODIFIED, i);
		if (!remove_file_and_path(&path, i))
			MARK_VOLUME_DELETE(vol);
		free(path.str);
	}

	/* Update metadata.  */
	if (!hashfile_opened_p(vol->metadata))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_METADATA);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	meta->dev = dev;
	meta->ino = ino;
	if (!hfile_lookup(vol->metadata, meta))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
	}
	if (meta->slot_status != VALID_SLOT)
	{
		meta->slot_status = VALID_SLOT;
		meta->dev = dev;
		meta->ino = ino;
		meta->gen = 1;
		zfs_fh_undefine(meta->master_fh);
	}

	map.master_fh = meta->master_fh;

	meta->flags = 0;
	meta->gen++;
	meta->local_version = 1;
	meta->master_version = vol->is_copy ? 0 : 1;
	zfs_fh_undefine(meta->master_fh);
	meta->modetype = GET_MODETYPE(0, FT_BAD);
	meta->parent_dev = (uint32_t) - 1;
	meta->parent_ino = (uint32_t) - 1;
	memset(meta->name, 0, METADATA_NAME_SIZE);

	if (!hfile_insert(vol->metadata, meta, false))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
	}

	zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);

	if (!zfs_fh_undefined(map.master_fh))
		RETURN_BOOL(delete_fh_mapping(vol, &map));
	RETURN_BOOL(true);
}

/*! Delete master fh and fh mapping for newly created file FH with metadata
   META on volume VOL.  */

bool delete_metadata_of_created_file(volume vol, zfs_fh * fh, metadata * meta)
{
	fh_mapping map;
	string path;
	int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!zfs_fh_undefined(meta->master_fh))
	{
		/* Delete the file handle mapping.  */
		if (!hashfile_opened_p(vol->fh_mapping))
		{
			int fd;

			fd = open_hash_file(vol, METADATA_TYPE_FH_MAPPING);
			if (fd < 0)
				RETURN_BOOL(false);
		}

		map.master_fh.dev = meta->master_fh.dev;
		map.master_fh.ino = meta->master_fh.ino;
		if (!hfile_delete(vol->fh_mapping, &map))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
			RETURN_BOOL(false);
		}
		zfsd_mutex_unlock(&metadata_fd_data[vol->fh_mapping->fd].mutex);
	}

	/* Delete hardlink list and interval files.  */
	delete_hardlinks_file(vol, fh);
	for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
	{
		build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_UPDATED, i);
		if (!remove_file_and_path(&path, i))
			MARK_VOLUME_DELETE(vol);
		free(path.str);
		build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_MODIFIED, i);
		if (!remove_file_and_path(&path, i))
			MARK_VOLUME_DELETE(vol);
		free(path.str);
	}

	/* Update metadata.  */
	meta->flags = 0;
	meta->gen++;
	meta->local_version = 1;
	meta->master_version = vol->is_copy ? 0 : 1;
	zfs_fh_undefine(meta->master_fh);
	meta->parent_dev = (uint32_t) - 1;
	meta->parent_ino = (uint32_t) - 1;
	memset(meta->name, 0, METADATA_NAME_SIZE);
	fh->gen = meta->gen;

	if (!hashfile_opened_p(vol->metadata))
	{
		int fd;

		fd = open_hash_file(vol, METADATA_TYPE_METADATA);
		if (fd < 0)
			RETURN_BOOL(false);
	}

	if (!hfile_insert(vol->metadata, meta, false))
	{
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
		RETURN_BOOL(false);
	}
	zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);

	RETURN_BOOL(true);
}

/*! Load interval trees for file handle FH on volume VOL. Return false on
   file error.  */

bool load_interval_trees(volume vol, internal_fh fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	fh->interval_tree_users++;
	if (fh->interval_tree_users > 1)
		RETURN_BOOL(true);

	if (!init_interval_tree(vol, fh, METADATA_TYPE_UPDATED))
	{
		fh->interval_tree_users--;
		RETURN_BOOL(false);
	}
	if (!init_interval_tree(vol, fh, METADATA_TYPE_MODIFIED))
	{
		fh->interval_tree_users--;
		close_interval_file(fh->updated);
		interval_tree_destroy(fh->updated);
		fh->updated = NULL;
		RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

/*! Save interval trees for file handle FH on volume VOL. Return false on
   file error.  */

bool save_interval_trees(volume vol, internal_fh fh)
{
	bool r;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

#ifdef ENABLE_CHECKING
	if (fh->interval_tree_users == 0)
		zfsd_abort();
#endif

	fh->interval_tree_users--;
	if (fh->interval_tree_users > 0)
		RETURN_BOOL(true);

#ifdef ENABLE_CHECKING
	if (!fh->updated)
		zfsd_abort();
	if (!fh->modified)
		zfsd_abort();
#endif

	r = free_interval_tree(vol, fh, METADATA_TYPE_UPDATED);
	r &= free_interval_tree(vol, fh, METADATA_TYPE_MODIFIED);

	RETURN_BOOL(r);
}

/*! Delete list of hardlinks of ZFS file handle FH on volume VOL.  */

static void delete_hardlinks_file(volume vol, zfs_fh * fh)
{
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	for (i = 0; i <= MAX_METADATA_TREE_DEPTH; i++)
	{
		string file;

		build_fh_metadata_path(&file, vol, fh, METADATA_TYPE_HARDLINKS, i);
		if (!remove_file_and_path(&file, get_metadata_tree_depth()))
			MARK_VOLUME_DELETE(vol);
		free(file.str);
	}

	RETURN_VOID;
}

/*! Read list of hardlinks from file descriptor FD to hardlink list HL.  */

static void read_hardlinks_file(hardlink_list hl, int fd)
{
	FILE *f;

	TRACE("");

	f = fdopen(fd, "rb");
#ifdef ENABLE_CHECKING
	if (!f)
		zfsd_abort();
#endif

	for (;;)
	{
		uint32_t parent_dev;
		uint32_t parent_ino;
		string name;

		if (fread(&parent_dev, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fread(&parent_ino, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fread(&name.len, 1, sizeof(uint32_t), f) != sizeof(uint32_t))
			break;

		parent_dev = le_to_u32(parent_dev);
		parent_ino = le_to_u32(parent_ino);
		name.len = le_to_u32(name.len);
		name.str = (char *)xmalloc(name.len + 1);

		if (fread(name.str, 1, name.len + 1, f) != name.len + 1)
		{
			free(name.str);
			break;
		}
		name.str[name.len] = 0;

		hardlink_list_insert(hl, parent_dev, parent_ino, &name, false);
	}

	fclose(f);
}

/*! Write hardlink list HL for file handle FH on volume VOL to hardlink file.
   Return false on file error.  */

static bool write_hardlinks_file(volume vol, zfs_fh * fh, hardlink_list hl)
{
	hardlink_list_entry entry;
	string path, new_path;
	int fd;
	FILE *f;

	TRACE("");

	build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_HARDLINKS,
						   get_metadata_tree_depth());
	append_string(&new_path, &path, ".new", 4);
	fd = open_fh_metadata(&new_path, vol, fh, METADATA_TYPE_HARDLINKS,
						  O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		free(new_path.str);
		free(path.str);
		RETURN_BOOL(false);
	}

	f = fdopen(fd, "wb");
#ifdef ENABLE_CHECKING
	if (!f)
		zfsd_abort();
#endif

	for (entry = hl->first; entry; entry = entry->next)
	{
		uint32_t parent_dev;
		uint32_t parent_ino;
		unsigned int name_len;

		parent_dev = u32_to_le(entry->parent_dev);
		parent_ino = u32_to_le(entry->parent_ino);
		name_len = u32_to_le(entry->name.len);
		if (fwrite(&parent_dev, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&parent_ino, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&name_len, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| (fwrite(entry->name.str, 1, entry->name.len + 1, f)
				!= entry->name.len + 1))
		{
			fclose(f);
			unlink(new_path.str);
			free(new_path.str);
			free(path.str);
			RETURN_BOOL(false);
		}
	}

	fclose(f);
	rename(new_path.str, path.str);
	free(new_path.str);
	free(path.str);
	RETURN_BOOL(true);
}

/*! Read hardlinks for file handle FH on volume VOL to hardlink list HL and
   the metadata to META.  */

bool read_hardlinks(volume vol, zfs_fh * fh, metadata * meta, hardlink_list hl)
{
	string name;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!lookup_metadata(vol, fh, meta, false))
		RETURN_BOOL(false);

	if (meta->slot_status != VALID_SLOT)
		RETURN_BOOL(true);

	if (GET_MODETYPE_TYPE(meta->modetype) == FT_BAD)
		RETURN_BOOL(true);

	if (meta->name[0] != 0 || (meta->parent_dev == 0 && meta->parent_ino == 0))
	{
#ifdef ENABLE_CHECKING
		if (meta->parent_dev == (uint32_t) - 1
			&& meta->parent_ino == (uint32_t) - 1)
			zfsd_abort();
#endif

		name.str = meta->name;
		name.len = strlen(meta->name);
		hardlink_list_insert(hl, meta->parent_dev, meta->parent_ino,
							 &name, true);
	}
	else
	{
		string path;
		int fd;

#ifdef ENABLE_CHECKING
		if (meta->parent_dev != (uint32_t) - 1
			|| meta->parent_ino != (uint32_t) - 1)
			zfsd_abort();
#endif

		build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_HARDLINKS,
							   get_metadata_tree_depth());
		fd = open_fh_metadata(&path, vol, fh, METADATA_TYPE_HARDLINKS,
							  O_RDONLY, S_IRUSR | S_IWUSR);
		free(path.str);

		if (fd >= 0)
			read_hardlinks_file(hl, fd);
	}

	RETURN_BOOL(true);
}

/*! Write the hardlink list HL for file handle FH on volume VOL either to
   hardlink file or to metadata file. Return false on file error.  */

static bool
write_hardlinks(volume vol, zfs_fh * fh, metadata * meta, hardlink_list hl)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (hl->first
		&& (hl->first->next || hl->first->name.len >= METADATA_NAME_SIZE))
	{
		if (!write_hardlinks_file(vol, fh, hl))
		{
			hardlink_list_destroy(hl);
			RETURN_BOOL(false);
		}
		hardlink_list_destroy(hl);

		if (!hashfile_opened_p(vol->metadata))
		{
			int fd;

			fd = open_hash_file(vol, METADATA_TYPE_METADATA);
			if (fd < 0)
				RETURN_BOOL(false);
		}

		if (meta->slot_status != VALID_SLOT)
		{
			meta->slot_status = VALID_SLOT;
			meta->dev = fh->dev;
			meta->ino = fh->ino;
			meta->gen = 1;
			meta->local_version = 1;
			meta->master_version = vol->is_copy ? 0 : 1;
			zfs_fh_undefine(meta->master_fh);
			meta->parent_dev = (uint32_t) - 1;
			meta->parent_ino = (uint32_t) - 1;
			memset(meta->name, 0, METADATA_NAME_SIZE);

			if (!hfile_insert(vol->metadata, meta, false))
			{
				zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
				RETURN_BOOL(false);
			}
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(true);
		}

		if (meta->name[0] == 0)
		{
#ifdef ENABLE_CHECKING
			if (meta->parent_dev != (uint32_t) - 1
				|| meta->parent_ino != (uint32_t) - 1)
				zfsd_abort();
#endif
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(true);
		}

#ifdef ENABLE_CHECKING
		if (meta->parent_dev == (uint32_t) - 1
			&& meta->parent_ino == (uint32_t) - 1)
			zfsd_abort();
#endif

		meta->parent_dev = (uint32_t) - 1;
		meta->parent_ino = (uint32_t) - 1;
		memset(meta->name, 0, METADATA_NAME_SIZE);

		if (!hfile_insert(vol->metadata, meta, false))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(false);
		}
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
	}
	else if (hl->first)
	{
		hardlink_list_entry entry;

		if (!hashfile_opened_p(vol->metadata))
		{
			int fd;

			fd = open_hash_file(vol, METADATA_TYPE_METADATA);
			if (fd < 0)
			{
				hardlink_list_destroy(hl);
				RETURN_BOOL(false);
			}
		}

		if (meta->slot_status != VALID_SLOT)
		{
			meta->slot_status = VALID_SLOT;
			meta->dev = fh->dev;
			meta->ino = fh->ino;
			meta->gen = 1;
			meta->local_version = 1;
			meta->master_version = vol->is_copy ? 0 : 1;
			zfs_fh_undefine(meta->master_fh);
		}

		entry = hl->first;
		meta->parent_dev = entry->parent_dev;
		meta->parent_ino = entry->parent_ino;
		memcpy(meta->name, entry->name.str, entry->name.len);
		memset(meta->name + entry->name.len, 0,
			   METADATA_NAME_SIZE - entry->name.len);

		hardlink_list_destroy(hl);

		if (!hfile_insert(vol->metadata, meta, false))
		{
			zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);
			RETURN_BOOL(false);
		}
		zfsd_mutex_unlock(&metadata_fd_data[vol->metadata->fd].mutex);

		delete_hardlinks_file(vol, fh);
	}
	else
	{
		hardlink_list_destroy(hl);
		delete_hardlinks_file(vol, fh);
	}

	RETURN_BOOL(true);
}

/*! Insert a hardlink [PARENT_DEV, PARENT_INO, NAME] to hardlink list for
   file handle FH on volume VOL.  Use META for loading metadata. Return false
   on file error.  */

bool
metadata_hardlink_insert(volume vol, zfs_fh * fh, metadata * meta,
						 uint32_t parent_dev, uint32_t parent_ino,
						 string * name)
{
	hardlink_list hl;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	hl = hardlink_list_create(2, NULL);
	if (!read_hardlinks(vol, fh, meta, hl))
	{
		hardlink_list_destroy(hl);
		RETURN_BOOL(false);
	}

	if (hardlink_list_insert(hl, parent_dev, parent_ino, name, true))
		RETURN_BOOL(write_hardlinks(vol, fh, meta, hl));

	hardlink_list_destroy(hl);
	RETURN_BOOL(true);
}

/*! Replace a hardlink [OLD_PARENT_DEV, OLD_PARENT_INO, OLD_NAME] by
   [NEW_PARENT_DEV, NEW_PARENT_INO, NEW_NAME] in hardlink list for file handle 
   FH on volume VOL.  Use META for loading metadata. If SHADOW is true set the 
   METADATA_SHADOW flag, otherwise clear it. Return false on file error.  */

bool
metadata_hardlink_replace(volume vol, zfs_fh * fh, metadata * meta,
						  uint32_t old_parent_dev, uint32_t old_parent_ino,
						  string * old_name, uint32_t new_parent_dev,
						  uint32_t new_parent_ino, string * new_name,
						  bool shadow)
{
	hardlink_list hl;
	bool flush;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	hl = hardlink_list_create(2, NULL);
	if (!read_hardlinks(vol, fh, meta, hl))
	{
		hardlink_list_destroy(hl);
		RETURN_BOOL(false);
	}

	if (shadow)
		meta->flags |= METADATA_SHADOW;
	else
		meta->flags &= ~METADATA_SHADOW;

	flush = hardlink_list_delete(hl, old_parent_dev, old_parent_ino, old_name);
	flush |= hardlink_list_insert(hl, new_parent_dev, new_parent_ino,
								  new_name, true);
	if (flush)
		RETURN_BOOL(write_hardlinks(vol, fh, meta, hl));

	hardlink_list_destroy(hl);
	RETURN_BOOL(true);
}

/*! Clear the hardlink list of file FH on volume VOL and add a hardlink
   specifying that the file is in shadow.  */

bool
metadata_hardlink_set(volume vol, zfs_fh * fh, metadata * meta,
					  uint32_t parent_dev, uint32_t parent_ino, string * name)
{
	hardlink_list hl;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	hl = hardlink_list_create(1, NULL);
	hardlink_list_insert(hl, parent_dev, parent_ino, name, true);

	RETURN_BOOL(write_hardlinks(vol, fh, meta, hl));
}

/*! Return the number of hardlinks of file FH on volume VOL and store the
   metadata to META.  */

unsigned int metadata_n_hardlinks(volume vol, zfs_fh * fh, metadata * meta)
{
	unsigned int n;
	hardlink_list hl;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	hl = hardlink_list_create(2, NULL);
	if (!read_hardlinks(vol, fh, meta, hl))
	{
		n = 0;
		MARK_VOLUME_DELETE(vol);
	}
	else
		n = hardlink_list_length(hl);
	hardlink_list_destroy(hl);

	RETURN_BOOL(n);
}

/*! Return a local path for file handle FH on volume VOL.  */

void get_local_path_from_metadata(string * path, volume vol, zfs_fh * fh)
{
	metadata meta;
	hardlink_list hl;
	hardlink_list_entry entry, next;
	string parent_path;
	struct stat st;
	bool flush;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	/* Get hardlink list.  */
	hl = hardlink_list_create(2, NULL);
	meta.modetype = GET_MODETYPE(0, FT_BAD);
	if (!read_hardlinks(vol, fh, &meta, hl))
	{
		MARK_VOLUME_DELETE(vol);
		hardlink_list_destroy(hl);
		path->str = NULL;
		path->len = 0;
		RETURN_VOID;
	}

	if (meta.slot_status != VALID_SLOT
		|| GET_MODETYPE_TYPE(meta.modetype) == FT_BAD)
	{
		hardlink_list_destroy(hl);
		path->str = NULL;
		path->len = 0;
		RETURN_VOID;
	}

	/* Check for volume root.  */
	if (meta.parent_dev == (uint32_t) - 1
		&& meta.parent_ino == (uint32_t) - 1
		&& meta.name[0] == 0 && hl->first == NULL)
	{
#ifdef ENABLE_CHECKING
		if ((meta.flags & METADATA_SHADOW) != 0)
			zfsd_abort();
#endif
		hardlink_list_destroy(hl);
		xstringdup(path, &vol->local_path);
		RETURN_VOID;
	}

	path->str = NULL;
	path->len = 0;
	flush = false;
	for (entry = hl->first; entry; entry = next)
	{
		zfs_fh parent_fh;

		next = entry->next;

		parent_fh.dev = entry->parent_dev;
		parent_fh.ino = entry->parent_ino;
		get_local_path_from_metadata(&parent_path, vol, &parent_fh);
		if (parent_path.str == NULL)
		{
			flush |= hardlink_list_delete_entry(hl, entry);
		}
		else
		{
			append_file_name(path, &parent_path, entry->name.str,
							 entry->name.len);
			free(parent_path.str);

			if (lstat(path->str, &st) != 0
				|| (uint32_t)st.st_dev !=  fh->dev || st.st_ino != fh->ino)
			{
				flush |= hardlink_list_delete_entry(hl, entry);

				free(path->str);
				path->str = NULL;
				path->len = 0;
			}
			else
				break;
		}
	}

	if (hl->first == NULL)
	{
#ifdef ENABLE_CHECKING
		if (path->str)
			zfsd_abort();
#endif

		if (!delete_metadata(vol, &meta, fh->dev, fh->ino, 0, 0, NULL))
			MARK_VOLUME_DELETE(vol);
	}

	if (flush)
	{
		if (!write_hardlinks(vol, fh, &meta, hl))
		{
			MARK_VOLUME_DELETE(vol);
			if (path->str)
			{
				free(path->str);
				path->str = NULL;
				path->len = 0;
			}
			RETURN_VOID;
		}
	}
	else
		hardlink_list_destroy(hl);

	RETURN_VOID;
}

/*! Write the journal JOURNAL for file handle FH on volume VOL to file PATH
   or delete the file if the journal is empty.  */

static bool
flush_journal(volume vol, zfs_fh * fh, journal_t journal, string * path)
{
	journal_entry entry;
	string new_path;
	int fd;
	FILE *f;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!journal)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(journal->mutex);

	close_journal_file(journal);

	if (journal->first == NULL)
	{
		bool r;

		r = remove_file_and_path(path, get_metadata_tree_depth());
		free(path->str);
		RETURN_BOOL(r);
	}

	append_string(&new_path, path, ".new", 4);
	fd = open_fh_metadata(&new_path, vol, fh, METADATA_TYPE_JOURNAL,
						  O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

	if (fd < 0)
	{
		free(new_path.str);
		free(path->str);
		RETURN_BOOL(false);
	}

	f = fdopen(fd, "wb");
#ifdef ENABLE_CHECKING
	if (!f)
		zfsd_abort();
#endif

	for (entry = journal->first; entry; entry = entry->next)
	{
		uint32_t dev;
		uint32_t ino;
		uint32_t gen;
		uint32_t oper;
		uint32_t name_len;
		zfs_fh master_fh;
		uint64_t master_version;

		dev = u32_to_le(entry->dev);
		ino = u32_to_le(entry->ino);
		gen = u32_to_le(entry->gen);
		oper = u32_to_le(entry->oper);
		name_len = u32_to_le(entry->name.len);
		master_fh.sid = u32_to_le(entry->master_fh.sid);
		master_fh.vid = u32_to_le(entry->master_fh.vid);
		master_fh.dev = u32_to_le(entry->master_fh.dev);
		master_fh.ino = u32_to_le(entry->master_fh.ino);
		master_fh.gen = u32_to_le(entry->master_fh.gen);
		master_version = u64_to_le(entry->master_version);
		if (fwrite(&dev, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&ino, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&gen, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&oper, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fwrite(&name_len, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| (fwrite(entry->name.str, 1, entry->name.len + 1, f)
				!= entry->name.len + 1)
			|| (fwrite(&master_fh, 1, sizeof(master_fh), f)
				!= sizeof(master_fh))
			|| (fwrite(&master_version, 1, sizeof(master_version), f)
				!= sizeof(master_version)))
		{
			fclose(f);
			unlink(new_path.str);
			free(new_path.str);
			free(path->str);
			RETURN_BOOL(false);
		}
	}

	fclose(f);
	rename(new_path.str, path->str);

	free(new_path.str);
	free(path->str);
	RETURN_BOOL(true);
}

/*! Read journal for file handle FH on volume VOL to JOURNAL.  */

bool read_journal(volume vol, zfs_fh * fh, journal_t journal)
{
	int fd;
	string path;
	FILE *f;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!journal)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(journal->mutex);

	build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_JOURNAL,
						   get_metadata_tree_depth());
	fd = open_fh_metadata(&path, vol, fh, METADATA_TYPE_JOURNAL, O_RDONLY, 0);
	if (fd < 0)
	{
		free(path.str);
		if (errno != ENOENT)
			RETURN_BOOL(false);

		RETURN_BOOL(true);
	}

	f = fdopen(fd, "rb");
#ifdef ENABLE_CHECKING
	if (!f)
		zfsd_abort();
#endif

	for (;;)
	{
		zfs_fh local_fh;
		zfs_fh master_fh;
		uint64_t master_version;
		uint32_t oper;
		string name;

		if (fread(&local_fh.dev, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| (fread(&local_fh.ino, 1, sizeof(uint32_t), f)
				!= sizeof(uint32_t))
			|| (fread(&local_fh.gen, 1, sizeof(uint32_t), f)
				!= sizeof(uint32_t))
			|| fread(&oper, 1, sizeof(uint32_t), f) != sizeof(uint32_t)
			|| fread(&name.len, 1, sizeof(uint32_t), f) != sizeof(uint32_t))
			break;

		local_fh.dev = le_to_u32(local_fh.dev);
		local_fh.ino = le_to_u32(local_fh.ino);
		local_fh.gen = le_to_u32(local_fh.gen);
		oper = le_to_u32(oper);
		name.len = le_to_u32(name.len);
		name.str = (char *)xmalloc(name.len + 1);

		if ((fread(name.str, 1, name.len + 1, f) != name.len + 1)
			|| (fread(&master_fh, 1, sizeof(master_fh), f)
				!= sizeof(master_fh))
			|| (fread(&master_version, 1, sizeof(master_version), f)
				!= sizeof(master_version)))
		{
			free(name.str);
			break;
		}
		name.str[name.len] = 0;
		master_fh.sid = le_to_u32(master_fh.sid);
		master_fh.vid = le_to_u32(master_fh.vid);
		master_fh.dev = le_to_u32(master_fh.dev);
		master_fh.ino = le_to_u32(master_fh.ino);
		master_fh.gen = le_to_u32(master_fh.gen);
		master_version = le_to_u64(master_version);

		if (oper < JOURNAL_OPERATION_LAST_AND_UNUSED)
		{
			journal_insert(journal, (journal_operation_t) oper,
						   &local_fh, &master_fh, master_version, &name,
						   false);
		}
		else
			free(name.str);
	}

	fclose(f);
	RETURN_BOOL(flush_journal(vol, fh, journal, &path));
}

/*! Write the journal JOURNAL for file handle FH on volume VOL to appropriate 
   file.  */

bool write_journal(volume vol, zfs_fh * fh, journal_t journal)
{
	string path;

	build_fh_metadata_path(&path, vol, fh, METADATA_TYPE_JOURNAL,
						   get_metadata_tree_depth());

	RETURN_BOOL(flush_journal(vol, fh, journal, &path));
}

/*! Add a journal entry with key [LOCAL_FH, NAME], master file handle
   MASTER_FH, master version MASTER_VERSION and operation OPER to journal
   JOURNAL for file handle FH on volume VOL.  */

bool
add_journal_entry(volume vol, journal_t journal, zfs_fh * fh,
				  zfs_fh * local_fh, zfs_fh * master_fh,
				  uint64_t master_version, string * name,
				  journal_operation_t oper)
{
	char buffer[ZFS_DC_SIZE];
	char *end;
	uint32_t tmp32;
	uint64_t tmp64;
	zfs_fh tmp_fh;
	bool r;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!journal)
		zfsd_abort();
	if (!vol->local_path.str || !vol->is_copy)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(journal->mutex);

	if (!journal_opened_p(journal))
	{
		if (open_journal_file(vol, journal, fh) < 0)
			RETURN_BOOL(false);
	}
	else
	{
		if (lseek(journal->fd, 0, SEEK_END) == (off_t) - 1)
		{
			message(LOG_ERROR, FACILITY_DATA, "lseek: %s\n", strerror(errno));
			zfsd_mutex_unlock(&metadata_fd_data[journal->fd].mutex);
			RETURN_BOOL(false);
		}
	}

#ifdef ENABLE_CHECKING
	if (name->len + 1 + 5 * sizeof(uint32_t) + sizeof(master_fh) > ZFS_DC_SIZE)
		zfsd_abort();
#endif

	end = buffer;

	tmp32 = u32_to_le(local_fh->dev);
	memcpy(end, &tmp32, sizeof(uint32_t));
	end += sizeof(uint32_t);

	tmp32 = u32_to_le(local_fh->ino);
	memcpy(end, &tmp32, sizeof(uint32_t));
	end += sizeof(uint32_t);

	tmp32 = u32_to_le(local_fh->gen);
	memcpy(end, &tmp32, sizeof(uint32_t));
	end += sizeof(uint32_t);

	tmp32 = u32_to_le(oper);
	memcpy(end, &tmp32, sizeof(uint32_t));
	end += sizeof(uint32_t);

	tmp32 = u32_to_le(name->len);
	memcpy(end, &tmp32, sizeof(uint32_t));
	end += sizeof(uint32_t);

	memcpy(end, name->str, name->len + 1);
	end += name->len + 1;

	tmp_fh.sid = u32_to_le(master_fh->sid);
	tmp_fh.vid = u32_to_le(master_fh->vid);
	tmp_fh.dev = u32_to_le(master_fh->dev);
	tmp_fh.ino = u32_to_le(master_fh->ino);
	tmp_fh.gen = u32_to_le(master_fh->gen);
	memcpy(end, &tmp_fh, sizeof(zfs_fh));
	end += sizeof(zfs_fh);

	tmp64 = u64_to_le(master_version);
	memcpy(end, &tmp64, sizeof(uint64_t));
	end += sizeof(uint64_t);

	r = full_write(journal->fd, buffer, end - buffer);
	zfsd_mutex_unlock(&metadata_fd_data[journal->fd].mutex);

	if (!r)
		RETURN_BOOL(false);

	journal_insert(journal, oper, local_fh, master_fh, master_version, name,
				   true);

	RETURN_BOOL(true);
}

/*! Add a journal entry for file with metadata META, name NAME and operation
   OPER to journal JOURNAL for file handle FH on volume VOL.  */

bool
add_journal_entry_meta(volume vol, journal_t journal, zfs_fh * fh,
					   metadata * meta, string * name,
					   journal_operation_t oper)
{
	zfs_fh local_fh;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (!vol->local_path.str || !vol->is_copy)
		zfsd_abort();
	if (meta->slot_status != VALID_SLOT)
		zfsd_abort();
#endif

	local_fh.dev = meta->dev;
	local_fh.ino = meta->ino;
	local_fh.gen = meta->gen;

	RETURN_BOOL(add_journal_entry(vol, journal, fh, &local_fh,
								  &meta->master_fh, meta->master_version,
								  name, oper));
}

/*! Build and create path PATH to shadow file for file FH with name NAME on
   volume VOL.  */

bool create_shadow_path(string * path, volume vol, zfs_fh * fh, string * name)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);

	build_shadow_metadata_path(path, vol, fh, name);
	if (!create_path_for_file(path, S_IRWXU | S_IRWXG | S_IRWXO, vol))
	{
		free(path->str);
		path->str = NULL;
		path->len = 0;
		RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

/*! Initialize data structures in METADATA.C.  */

void initialize_metadata_c(void)
{
	int i;

	zfsd_mutex_init(&metadata_mutex);
	metadata_heap = fibheap_new(max_metadata_fds, &metadata_mutex);

	/* Data for each file descriptor.  */
	metadata_fd_data
		= (metadata_fd_data_t *) xcalloc(max_nfd, sizeof(metadata_fd_data_t));
	for (i = 0; i < max_nfd; i++)
	{
		zfsd_mutex_init(&metadata_fd_data[i].mutex);
		metadata_fd_data[i].fd = -1;
	}
}

/*! Destroy data structures in METADATA.C.  */

void cleanup_metadata_c(void)
{
	while (fibheap_size(metadata_heap) > 0)
	{
		metadata_fd_data_t *fd_data;

		zfsd_mutex_lock(&metadata_mutex);
		fd_data = (metadata_fd_data_t *) fibheap_extract_min(metadata_heap);
#ifdef ENABLE_CHECKING
		if (!fd_data && fibheap_size(metadata_heap) > 0)
			zfsd_abort();
#endif
		if (fd_data)
		{
			zfsd_mutex_lock(&fd_data->mutex);
			fd_data->heap_node = NULL;
			if (fd_data->fd >= 0)
				close_metadata_fd(fd_data->fd);
			else
				zfsd_mutex_unlock(&fd_data->mutex);
		}
		zfsd_mutex_unlock(&metadata_mutex);
	}
	zfsd_mutex_lock(&metadata_mutex);
	fibheap_delete(metadata_heap);
	zfsd_mutex_unlock(&metadata_mutex);
	zfsd_mutex_destroy(&metadata_mutex);
	free(metadata_fd_data);
}
