/* ! \file \brief Volume functions.  */

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
#include <stdlib.h>
#include "pthread-wrapper.h"
#include "fh.h"
#include "hashtab.h"
#include "hashfile.h"
#include "memory.h"
#include "volume.h"
#include "network.h"
#include "metadata.h"

/* ! Hash table of volumes, searched by ID.  */
static htab_t volume_htab;

/* ! Hash table of volumes, searched by NAME.  */
static htab_t volume_htab_name;

/* ! Mutex for table of volumes.  */
pthread_mutex_t volume_mutex;

bool is_valid_volume_id(uint32_t vid)
{
	return (vid != 0) && (vid != (uint32_t) - 1);
}

bool is_valid_volume_name(const char * name)
{
	// strlen(name) > 0
	return (name != NULL && name[0] != 0);
}

/* ! Hash function for volume ID ID.  */
#define HASH_VOLUME_ID(ID) (ID)

/* ! Hash function for volume N.  */
#define VOLUME_HASH(V) ((V)->id)

/* ! Hash function for volume name NAME.  */
#define HASH_VOLUME_NAME(NAME) crc32_buffer ((NAME).str, (NAME).len)

/* ! Hash function for volume V.  */
#define VOLUME_HASH_NAME(V) HASH_VOLUME_NAME ((V)->name)

/* ! Hash function for volume X, conputed from ID.  */

static hash_t volume_hash(const void *x)
{
	return VOLUME_HASH((const struct volume_def *)x);
}

/* ! Hash function for volume X, conputed from volume name.  */

static hash_t volume_hash_name(const void *x)
{
	return VOLUME_HASH_NAME((const struct volume_def *)x);
}

/* ! Compare a volume X with ID *Y.  */

static int volume_eq(const void *x, const void *y)
{
	const struct volume_def *vol = (const struct volume_def *)x;
	uint32_t id = *(const uint32_t *)y;

	return vol->id == id;
}

/* ! Compare a volume X with string Y.  */

static int volume_eq_name(const void *x, const void *y)
{
	const struct volume_def *vol = (const struct volume_def *)x;
	const string *s = (const string *)y;

	return (vol->name.len == s->len && strcmp(vol->name.str, s->str) == 0);
}

/* ! Return the volume with volume ID == ID.  */

volume volume_lookup(uint32_t id)
{
	volume vol;

	zfsd_mutex_lock(&volume_mutex);
	vol = (volume) htab_find_with_hash(volume_htab, &id, HASH_VOLUME_ID(id));
	if (vol)
		zfsd_mutex_lock(&vol->mutex);
	zfsd_mutex_unlock(&volume_mutex);

	return vol;
}

/* ! Return the volume with volume ID == ID.  */

volume volume_lookup_nolock(uint32_t id)
{
	volume vol;

	CHECK_MUTEX_LOCKED(&volume_mutex);

	vol = (volume) htab_find_with_hash(volume_htab, &id, HASH_VOLUME_ID(id));
	if (vol)
		zfsd_mutex_lock(&vol->mutex);

	return vol;
}

/* ! Return the volume with name == NAME.  */

volume volume_lookup_name(string * name)
{
	volume vol;

	zfsd_mutex_lock(&volume_mutex);
	vol = (volume) htab_find_with_hash(volume_htab_name, name,
									   HASH_VOLUME_NAME(*name));
	if (vol)
		zfsd_mutex_lock(&vol->mutex);
	zfsd_mutex_unlock(&volume_mutex);

	return vol;
}

/* ! Create volume structure and fill it with information.  */

volume volume_create(uint32_t id)
{
	volume vol;
	void **slot;

	CHECK_MUTEX_LOCKED(&volume_mutex);

	vol = (volume) xmalloc(sizeof(struct volume_def));
	vol->id = id;
	vol->master = NULL;
	vol->slaves = NULL;
	vol->name = invalid_string;
	vol->mountpoint = invalid_string;
	vol->delete_p = false;
	vol->marked = false;
	vol->n_locked_fhs = 0;
	vol->local_path = invalid_string;
	vol->size_limit = VOLUME_NO_LIMIT;
	vol->last_conflict_ino = 0;
	vol->root_dentry = NULL;
	vol->root_vd = NULL;
	vol->metadata = NULL;
	vol->fh_mapping = NULL;

	zfsd_mutex_init(&vol->mutex);
	zfsd_mutex_lock(&vol->mutex);

	/* Create the list of nodes whose master is this node.  */
	if (id == VOLUME_ID_CONFIG)
	{
		vol->slaves = htab_create(5, node_hash_name, node_eq_name, NULL,
								  &vol->mutex);
	}

	slot = htab_find_slot_with_hash(volume_htab, &vol->id, VOLUME_HASH(vol),
									INSERT);
#ifdef ENABLE_CHECKING
	if (slot == EMPTY_ENTRY || *slot)
		abort();
#endif
	*slot = vol;

	return vol;
}

/* ! Destroy volume VOL and free memory associated with it. This function
   expects volume_mutex to be locked.  */

static void volume_destroy(volume vol)
{
	void **slot;

	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&volume_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

#ifdef ENABLE_CHECKING
	if (vol->n_locked_fhs > 0)
		abort();
#endif

	if (vol->slaves)
		htab_destroy(vol->slaves);

	if (vol->root_dentry)
	{
		uint32_t vid;
		internal_dentry dentry;

		vid = vol->id;
		dentry = vol->root_dentry;
		zfsd_mutex_lock(&dentry->fh->mutex);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&volume_mutex);
		internal_dentry_destroy(dentry, true, false, true);
		zfsd_mutex_lock(&volume_mutex);
		vol = volume_lookup_nolock(vid);
		if (!vol)
		{
			/* The volume is already destroyed.  */
			return;
		}
	}

	virtual_mountpoint_destroy(vol);

	close_volume_metadata(vol);

	slot = htab_find_slot_with_hash(volume_htab, &vol->id, VOLUME_HASH(vol),
									NO_INSERT);
#ifdef ENABLE_CHECKING
	if (slot == EMPTY_ENTRY)
		abort();
#endif
	htab_clear_slot(volume_htab, slot);
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_destroy(&vol->mutex);

	if (vol->local_path.str)
		free(vol->local_path.str);
	if (vol->mountpoint.str)
		free(vol->mountpoint.str);
	if (vol->name.str)
		free(vol->name.str);
	free(vol);
}

/* ! Destroy volume VOL and free memory associated with it. Destroy dentries
   while volume_mutex is unlocked. This function expects fh_mutex to be
   locked.  */

void volume_delete(volume vol)
{
	uint32_t vid = vol->id;

	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

#ifdef ENABLE_CHECKING
	if (vol->n_locked_fhs > 0)
		abort();
#endif

	/* Destroy dentries on volume.  */
	if (vol->root_dentry)
	{
		internal_dentry dentry;

		dentry = vol->root_dentry;
		zfsd_mutex_lock(&dentry->fh->mutex);
		zfsd_mutex_unlock(&vol->mutex);
		internal_dentry_destroy(dentry, true, false, true);
	}
	else
		zfsd_mutex_unlock(&vol->mutex);

	/* Destroy volume.  */
	zfsd_mutex_unlock(&fh_mutex);
	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	vol = volume_lookup_nolock(vid);
	if (vol)
		volume_destroy(vol);
	zfsd_mutex_unlock(&volume_mutex);
}

/* ! Set the information common for all volume types.  */

void
volume_set_common_info(volume vol, string * name, string * mountpoint,
					   node master)
{
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&volume_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (vol->name.len != name->len
		|| vol->name.str == NULL || strcmp(vol->name.str, name->str) != 0)
	{
		void **slot;

		if (vol->name.str)
		{
			slot = htab_find_slot_with_hash(volume_htab_name, &vol->name,
											HASH_VOLUME_NAME(vol->name),
											NO_INSERT);
#ifdef ENABLE_CHECKING
			if (!slot)
				abort();
#endif
			htab_clear_slot(volume_htab_name, slot);

			free(vol->name.str);
			vol->name.str = NULL;
			vol->name.len = 0;
		}

		slot = htab_find_slot_with_hash(volume_htab_name, name,
										HASH_VOLUME_NAME(*name), INSERT);
		if (*slot)
		{
			vol->marked = true;
			message(LOG_NOTICE, FACILITY_DATA | FACILITY_CONFIG,
					"Volume with name = %s already exists\n", name->str);
			return;
		}
		*slot = vol;

		set_string(&vol->name, name);
	}

	vol->marked = false;
	vol->master = master;
	vol->is_copy = (vol->master != this_node);
	if (vol->mountpoint.len != mountpoint->len
		|| strcmp(vol->mountpoint.str, mountpoint->str) != 0)
	{
		virtual_mountpoint_destroy(vol);
		set_string(&vol->mountpoint, mountpoint);
		virtual_mountpoint_create(vol);
	}
}

/* ! Wrapper for volume_set_common_info.  */

void
volume_set_common_info_wrapper(volume vol, char *name, char *mountpoint,
							   node master)
{
	string name_str;
	string mountpoint_str;

	name_str.str = name;
	name_str.len = strlen(name);
	mountpoint_str.str = mountpoint;
	mountpoint_str.len = strlen(mountpoint);
	volume_set_common_info(vol, &name_str, &mountpoint_str, master);
}

/* ! Set the information for a volume with local copy. \param volp Volume.
   \param local_path Local path to the volume. \param size_limit Size limit
   for the volume.  */

bool
volume_set_local_info(volume * volp, string * local_path, uint64_t size_limit)
{
	volume vol = *volp;

	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (vol->local_path.len != local_path->len
		|| vol->local_path.str == NULL
		|| strcmp(vol->local_path.str, local_path->str) != 0)
	{
		uint32_t vid;
		internal_dentry dentry;

		if (vol->root_dentry)
		{
			/* We are changing the local path, so delete all dentries.  */
			vid = vol->id;
			dentry = vol->root_dentry;
			zfsd_mutex_lock(&dentry->fh->mutex);
			zfsd_mutex_unlock(&vol->mutex);
			internal_dentry_destroy(dentry, true, false, true);
			*volp = vol = volume_lookup(vid);
			if (!vol)
			{
				/* The volume was destroyed.  */
				return true;
			}
		}

		set_string(&vol->local_path, local_path);
	}

	vol->size_limit = size_limit;

	close_volume_metadata(vol);
	vol->delete_p = false;
	return init_volume_metadata(vol);
}

/* ! Set the information for a volume with local copy. \param volp Volume.
   \param local_path Local path to the volume. \param size_limit Size limit
   for the volume.  */

bool
volume_set_local_info_wrapper(volume * volp, char *local_path,
							  uint64_t size_limit)
{
	string local_path_str;

	local_path_str.str = local_path;
	local_path_str.len = strlen(local_path);
	return volume_set_local_info(volp, &local_path_str, size_limit);
}

// TODO: remve this unused function
#if 0
/* ! Print the information about volume VOL to file F.  */

static void print_volume(FILE * f, volume vol)
{
	fprintf(f, "%u %s %s\n", vol->id, vol->name.str, vol->mountpoint.str);
}


/* ! Print the information about all volumes to file F.  */

static void print_volumes(FILE * f)
{
	void **slot;

	HTAB_FOR_EACH_SLOT(volume_htab, slot)
	{
		print_volume(f, (volume) * slot);
	}
}
#endif

/* ! Mark all volumes.  */

void mark_all_volumes(void)
{
	void **slot;

	zfsd_mutex_lock(&volume_mutex);
	HTAB_FOR_EACH_SLOT(volume_htab, slot)
	{
		volume vol = (volume) * slot;

		zfsd_mutex_lock(&vol->mutex);
		vol->marked = true;
		zfsd_mutex_unlock(&vol->mutex);
	}
	zfsd_mutex_unlock(&volume_mutex);
}

/* ! Delete all dentries of marked volume and clear local path. \param vol
   Volume on which the dentries will be deleted.  */

static void delete_dentries_of_marked_volume(volume vol)
{
	internal_dentry dentry;
	uint32_t vid;

	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (!vol->marked)
	{
		zfsd_mutex_unlock(&vol->mutex);
		return;
	}

	if (vol->root_dentry)
	{
		vid = vol->id;
		dentry = vol->root_dentry;
		zfsd_mutex_lock(&dentry->fh->mutex);
		zfsd_mutex_unlock(&vol->mutex);
		internal_dentry_destroy(dentry, true, false, true);

		vol = volume_lookup(vid);
		if (!vol)
			return;
	}

	vol->local_path = invalid_string;
	close_volume_metadata(vol);
	vol->delete_p = false;
	vol->marked = false;
	zfsd_mutex_unlock(&vol->mutex);
}

/* ! Delete all dentries of marked volume.  */

void delete_dentries_of_marked_volumes(void)
{
	void **slot;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	HTAB_FOR_EACH_SLOT(volume_htab, slot)
	{
		volume vol = (volume) * slot;

		zfsd_mutex_lock(&vol->mutex);
		zfsd_mutex_unlock(&volume_mutex);
		delete_dentries_of_marked_volume(vol);
		zfsd_mutex_lock(&volume_mutex);
	}
	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);
}

/* ! Destroy volume VOL if it is marked.  */

static void destroy_marked_volume_1(volume vol)
{
	void **slot;
	bool master_marked;

	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&volume_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (vol->marked || vol->master == NULL)
		volume_destroy(vol);
	else
	{
		zfsd_mutex_lock(&node_mutex);
		zfsd_mutex_lock(&vol->master->mutex);
		master_marked = vol->master->marked;
		zfsd_mutex_unlock(&vol->master->mutex);
		zfsd_mutex_unlock(&node_mutex);
		if (master_marked)
			volume_destroy(vol);
		else
		{
			if (vol->slaves)
			{
				/* Delete the marked "slaves".  */
				zfsd_mutex_lock(&node_mutex);
				HTAB_FOR_EACH_SLOT(vol->slaves, slot)
				{
					node nod = (node) * slot;

					zfsd_mutex_lock(&nod->mutex);
					if (nod->marked)
						htab_clear_slot(vol->slaves, slot);
					zfsd_mutex_unlock(&nod->mutex);
				}
				zfsd_mutex_unlock(&node_mutex);
			}
			zfsd_mutex_unlock(&vol->mutex);
		}
	}
}

/* ! Destroy volume VID if it is marked.  */

void destroy_marked_volume(uint32_t vid)
{
	volume vol;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	vol = volume_lookup_nolock(vid);
	if (vol)
		destroy_marked_volume_1(vol);
	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);
}

/* ! Delete marked volumes.  */

void destroy_marked_volumes(void)
{
	void **slot;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	HTAB_FOR_EACH_SLOT(volume_htab, slot)
	{
		volume vol = (volume) * slot;

		zfsd_mutex_lock(&vol->mutex);
		destroy_marked_volume_1(vol);
	}
	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);
}

/* ! Delete all volumes.  */

void destroy_all_volumes(void)
{
	void **slot;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);
	HTAB_FOR_EACH_SLOT(volume_htab, slot)
	{
		volume vol = (volume) * slot;

		zfsd_mutex_lock(&vol->mutex);
		volume_destroy((volume) * slot);
	}
	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);
}

/* ! Initialize data structures in VOLUME.C.  */

void initialize_volume_c(void)
{
	zfsd_mutex_init(&volume_mutex);
	volume_htab =
		htab_create(200, volume_hash, volume_eq, NULL, &volume_mutex);
	volume_htab_name =
		htab_create(200, volume_hash_name, volume_eq_name, NULL,
					&volume_mutex);
}

/* ! Destroy data structures in VOLUME.C.  */

void cleanup_volume_c(void)
{
	destroy_all_volumes();
	zfsd_mutex_lock(&volume_mutex);
	htab_destroy(volume_htab);
	htab_destroy(volume_htab_name);
	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_destroy(&volume_mutex);
}
