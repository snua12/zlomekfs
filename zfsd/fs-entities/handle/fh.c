/*! \file \brief File handle functions.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak, Ales Snuparek

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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "pthread-wrapper.h"
#include "fh.h"
#include "cap.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "fibheap.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "varray.h"
#include "journal.h"
#include "metadata.h"
#include "zfs-prot.h"
#include "user-group.h"
#include "dir.h"
#include "update.h"
#include "configuration.h"
#include "version.h"
#include "fs-iface.h"

/*! File handle of ZFS root.  */
zfs_fh root_fh =
	{ NODE_ID_NONE, VOLUME_ID_VIRTUAL, VIRTUAL_DEVICE, ROOT_INODE, 1 };

/*! Static undefined ZFS file handle.  */
zfs_fh undefined_fh;

/*! The virtual directory root.  */
static virtual_dir root;

/*! Allocation pool for file handles.  */
static alloc_pool fh_pool;

/*! Allocation pool for dentries.  */
static alloc_pool dentry_pool;

/*! Hash table of used file handles, searched by local_fh.  */
htab_t fh_htab;

/*! Hash table of used dentries, searched by fh->local_fh.  */
htab_t dentry_htab;

/*! Hash table of used dentries, searched by (parent->fh->local_fh, name).  */
htab_t dentry_htab_name;

/*! Allocation pool for virtual directories ("mountpoints").  */
static alloc_pool vd_pool;

/*! Hash table of virtual directories, searched by fh.  */
static htab_t vd_htab;

/*! Hash table of virtual directories, searched by (parent->fh, name).  */
static htab_t vd_htab_name;

/*! Mutes for file handles, dentries and virtual directories.  */
pthread_mutex_t fh_mutex;

/*! Key for array of locked file handles.  */
static pthread_key_t lock_info_key;

/*! Heap holding internal file handles will be automatically freed when
   unused for a long time.  */
fibheap cleanup_dentry_heap;

/*! Mutex protecting CLEANUP_FH_*.  */
pthread_mutex_t cleanup_dentry_mutex = ZFS_MUTEX_INITIALIZER;

/*! Thread ID of thread freeing file handles unused for a long time.  */
pthread_t cleanup_dentry_thread;

/*! This mutex is locked when cleanup fh thread is in sleep.  */
pthread_mutex_t cleanup_dentry_thread_in_syscall = ZFS_MUTEX_INITIALIZER;

/*! Hash function for internal file handle FH.  */
#define INTERNAL_FH_HASH(FH)						\
  (ZFS_FH_HASH (&(FH)->local_fh))

/*! Hash function for virtual_dir VD, computed from fh.  */
#define VIRTUAL_DIR_HASH(VD)						\
  (ZFS_FH_HASH (&(VD)->fh))

/*! Hash function for virtual_dir VD, computed from (parent->fh, name).  */
#define VIRTUAL_DIR_HASH_NAME(VD)					\
  (crc32_update (crc32_buffer ((VD)->name.str, (VD)->name.len),		\
                 &(VD)->parent->fh, sizeof (zfs_fh)))

static internal_dentry make_space_in_conflict_dir(volume * volp,
												  internal_dentry * conflictp,
												  bool exists, zfs_fh * fh);

/*! Dentries which should never be cleaned up.  */
#define DENTRY_NEVER_CLEANUP(DENTRY)					\
  ((DENTRY)->next == (DENTRY)						\
   && ((DENTRY)->fh->cap != NULL					\
       || (DENTRY)->fh->level != LEVEL_UNLOCKED				\
       || (DENTRY)->fh->reintegrating_sid != 0))

/*! Return the fibheap key for dentry DENTRY.  */

static fibheapkey_t dentry_key(internal_dentry dentry)
{
	if (CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		internal_dentry tmp;
		unsigned int i;
		fibheapkey_t max_key = FIBHEAPKEY_MIN;

		for (i = 0; i < VARRAY_USED(dentry->fh->subdentries); i++)
		{
			tmp = VARRAY_ACCESS(dentry->fh->subdentries, i, internal_dentry);
			if (DENTRY_NEVER_CLEANUP(tmp))
				return FIBHEAPKEY_MAX;

			if (max_key < (fibheapkey_t) dentry->last_use)
				max_key = (fibheapkey_t) dentry->last_use;
		}

		return max_key;
	}

	if (DENTRY_NEVER_CLEANUP(dentry))
		return FIBHEAPKEY_MAX;

	return (fibheapkey_t) dentry->last_use;
}

/*! Return true if dentry DENTRY should have a node in CLEANUP_DENTRY_HEAP.  */

static bool dentry_should_have_cleanup_node(internal_dentry dentry)
{
	TRACE("%p", (void *)dentry);

	/* Root dentry can't be deleted.  */
	if (!dentry->parent)
		RETURN_BOOL(false);

	if (dentry->deleted)
		RETURN_BOOL(false);

	if (CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		internal_dentry tmp;
		unsigned int i;

		for (i = 0; i < VARRAY_USED(dentry->fh->subdentries); i++)
		{
			tmp = VARRAY_ACCESS(dentry->fh->subdentries, i, internal_dentry);
			if (tmp->fh->attr.type == FT_DIR
				&& VARRAY_USED(tmp->fh->subdentries) != 0)
				RETURN_BOOL(false);
		}

		RETURN_BOOL(true);
	}

	/* Directory dentry which has some subdentries can't be deleted.  */
	if (dentry->fh->attr.type == FT_DIR
		&& VARRAY_USED(dentry->fh->subdentries) != 0)
		RETURN_BOOL(false);

	RETURN_BOOL(true);
}

/*! Update the cleanup node of dentry DENTRY.  */

static void dentry_update_cleanup_node(internal_dentry dentry)
{
	TRACE("%p", (void *)dentry);
#ifdef ENABLE_CHECKING
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
#endif

	if (dentry->parent && CONFLICT_DIR_P(dentry->parent->fh->local_fh))
	{
		zfsd_mutex_lock(&cleanup_dentry_mutex);
		if (dentry->heap_node)
		{
			fibheap_delete_node(cleanup_dentry_heap, dentry->heap_node);
			dentry->heap_node = NULL;
		}
		zfsd_mutex_unlock(&cleanup_dentry_mutex);
		dentry = dentry->parent;
	}

	dentry->last_use = time(NULL);
	zfsd_mutex_lock(&cleanup_dentry_mutex);
	if (dentry_should_have_cleanup_node(dentry))
	{
		if (dentry->heap_node)
		{
			fibheap_replace_key(cleanup_dentry_heap, dentry->heap_node,
								dentry_key(dentry));
		}
		else
		{
			dentry->heap_node = fibheap_insert(cleanup_dentry_heap,
											   dentry_key(dentry), dentry);
		}
	}
	else
	{
		if (dentry->heap_node)
		{
			fibheap_delete_node(cleanup_dentry_heap, dentry->heap_node);
			dentry->heap_node = NULL;
		}
	}
	zfsd_mutex_unlock(&cleanup_dentry_mutex);
}

/*! Compare the volume IDs of ZFS_FHs P1 and P2.  */

static int cleanup_unused_dentries_compare(const void *p1, const void *p2)
{
	const zfs_fh *fh1 = (const zfs_fh *)p1;
	const zfs_fh *fh2 = (const zfs_fh *)p2;

	if (fh1->vid == fh2->vid)
		return 0;
	else if (fh1->vid < fh2->vid)
		return -1;
	return 1;
}

/*! Free internal dentries unused for at least
   MAX_INTERNAL_DENTRY_UNUSED_TIME seconds.  */

static void cleanup_unused_dentries(void)
{
	fibheapkey_t threshold;
	internal_dentry dentry;
	zfs_fh fh[1024];
	int i, n;

	threshold = (fibheapkey_t) time(NULL);
	if (threshold <= MAX_INTERNAL_DENTRY_UNUSED_TIME)
		threshold = 0;
	else
		threshold -= MAX_INTERNAL_DENTRY_UNUSED_TIME;

	do
	{
		zfsd_mutex_lock(&cleanup_dentry_mutex);
		for (n = 0; n < 1024; n++)
		{
			if (cleanup_dentry_heap->nodes == 0)
				break;

			dentry = (internal_dentry) fibheap_min(cleanup_dentry_heap);
#ifdef ENABLE_CHECKING
			if (!dentry)
				zfsd_abort();
#endif
			if (fibheap_min_key(cleanup_dentry_heap) >= threshold)
				break;

			fibheap_extract_min(cleanup_dentry_heap);

			/* We have to clear DENTRY->HEAP_NODE while the
			   CLEANUP_DENTRY_MUTEX is still locked. Moreover we have to copy
			   the ZFS_FH because the internal dentry may be freed as soon as
			   we unlock CLEANUP_DENTRY_MUTEX.  Later we have to lookup the
			   internal dentry and do nothing if it already does not exist.  */
			dentry->heap_node = NULL;
			fh[n] = dentry->fh->local_fh;
		}
		zfsd_mutex_unlock(&cleanup_dentry_mutex);
		if (n)
		{
			message(LOG_DEBUG, FACILITY_DATA, "Freeing %d nodes\n", n);
			qsort(fh, n, sizeof(zfs_fh), cleanup_unused_dentries_compare);

			for (i = 0; i < n; i++)
			{
				zfsd_mutex_lock(&fh_mutex);

				dentry = dentry_lookup(&fh[i]);
				if (!dentry)
				{
					zfsd_mutex_unlock(&fh_mutex);
					continue;
				}

				/* We may have added a dentry to it while CLEANUP_DENTRY_MUTEX 
				   was unlocked.  */
				if (dentry_should_have_cleanup_node(dentry))
				{
					release_dentry(dentry);
					zfsd_mutex_unlock(&fh_mutex);
					continue;
				}

				/* We may have looked up DENTRY again so we may have updated
				   LAST_USE or there are capabilities associated with the file 
				   handle and this is its only dentry.  */
				if (dentry_key(dentry) >= threshold)
				{
					/* Reinsert the file handle to heap.  */
					dentry_update_cleanup_node(dentry);
					release_dentry(dentry);
					zfsd_mutex_unlock(&fh_mutex);
					continue;
				}

				internal_dentry_destroy(dentry, true, false,
										dentry->parent == NULL);
				zfsd_mutex_unlock(&fh_mutex);
			}
		}
	}
	while (n > 0);
}

/*! Main function of thread freeing file handles unused for a long time.  */

static void *cleanup_dentry_thread_main(ATTRIBUTE_UNUSED void *data)
{
	thread_disable_signals();
	pthread_setspecific(thread_name_key, "IFH cleanup thread");

	while (keep_running())
	{
		zfsd_mutex_lock(&cleanup_dentry_thread_in_syscall);
		if (keep_running())
			sleep(1);
		zfsd_mutex_unlock(&cleanup_dentry_thread_in_syscall);
		if (!keep_running())
			break;

		cleanup_unused_dentries();
	}

	return NULL;
}

/*! Hash function for internal file handle X.  */
static hash_t internal_fh_hash(const void *x)
{
	return INTERNAL_FH_HASH((const struct internal_fh_def *)x);
}

/*! Hash function for internal dentry X, computed from fh->local_fh.  */

static hash_t internal_dentry_hash(const void *x)
{
	return INTERNAL_DENTRY_HASH((const struct internal_dentry_def *)x);
}

/*! Hash function for internal dentry X, computed from parent->fh and name.  */

static hash_t internal_dentry_hash_name(const void *x)
{
	return INTERNAL_DENTRY_HASH_NAME((const struct internal_dentry_def *)x);
}

/*! Compare an internal file handle XX with client's file handle YY.  */

static int internal_fh_eq(const void *xx, const void *yy)
{
	const zfs_fh *x = &((const struct internal_fh_def *)xx)->local_fh;
	const zfs_fh *y = (const zfs_fh *)yy;

	return (x->ino == y->ino && x->dev == y->dev
			&& x->vid == y->vid && x->sid == y->sid && x->gen == y->gen);
}

/*! Set array of lock info for current thread to LI. */

void set_lock_info(lock_info * li)
{
	int i;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (pthread_setspecific(lock_info_key, li))
		zfsd_abort();
#else
	pthread_setspecific(lock_info_key, li);
#endif

	for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
	{
		li[i].dentry = NULL;
		li[i].level = LEVEL_UNLOCKED;
	}
}

/*! Add dentry DENTRY locked to level LEVEL to list of dentries owned by
   current thread.  */

void set_owned(internal_dentry dentry, unsigned int level)
{
	lock_info *li;
	int i;

	TRACE("%p %u", (void *)dentry, level);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	li = (lock_info *) pthread_getspecific(lock_info_key);
#ifdef ENABLE_CHECKING
	if (level != LEVEL_SHARED && level != LEVEL_EXCLUSIVE)
		zfsd_abort();
	if (!li)
		zfsd_abort();
#endif

	for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
	{
		if (li[i].dentry == NULL)
		{
#ifdef ENABLE_CHECKING
			if (li[i].level != LEVEL_UNLOCKED)
				zfsd_abort();
#endif
			li[i].dentry = dentry;
			li[i].level = level;
			RETURN_VOID;
		}
	}

#ifdef ENABLE_CHECKING
	zfsd_abort();
#endif
}

/*! Remove dentry DENTRY from list of dentries owned by current thread.  */

static void clear_owned(internal_dentry dentry)
{
	lock_info *li;
	int i;

	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	li = (lock_info *) pthread_getspecific(lock_info_key);
#ifdef ENABLE_CHECKING
	if (!li)
		zfsd_abort();
#endif

	for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
	{
		if (li[i].dentry == dentry)
		{
#ifdef ENABLE_CHECKING
			if (li[i].level != LEVEL_SHARED && li[i].level != LEVEL_EXCLUSIVE)
				zfsd_abort();
#endif
			li[i].dentry = NULL;
			li[i].level = LEVEL_UNLOCKED;
			RETURN_VOID;
		}
	}

#ifdef ENABLE_CHECKING
	zfsd_abort();
#endif
}

/*! Return true if dentry DENTRY is owned by current thread.  */

static bool is_owned(internal_dentry dentry)
{
	lock_info *li;
	int i;

	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	li = (lock_info *) pthread_getspecific(lock_info_key);
#ifdef ENABLE_CHECKING
	if (!li)
		zfsd_abort();
#endif

	for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
	{
		if (li[i].dentry == dentry)
			RETURN_BOOL(true);
	}

	RETURN_BOOL(false);
}

/*! Return the level which dentry DENTRY is locked by current thread.  */

static unsigned int get_level(internal_dentry dentry)
{
	lock_info *li;
	int i;

	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	li = (lock_info *) pthread_getspecific(lock_info_key);
#ifdef ENABLE_CHECKING
	if (!li)
		zfsd_abort();
#endif

	for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
	{
		if (li[i].dentry == dentry)
			RETURN_INT(li[i].level);
	}

	RETURN_INT(LEVEL_UNLOCKED);
}

/*! Compare an internal file handle XX with client's file handle YY.  */

static int internal_dentry_eq(const void *xx, const void *yy)
{
	zfs_fh *x = &((const struct internal_dentry_def *)xx)->fh->local_fh;
	const zfs_fh *y = (const zfs_fh *)yy;

	return (x->ino == y->ino && x->dev == y->dev
			&& x->vid == y->vid && x->sid == y->sid && x->gen == y->gen);
}

/*! Compare two internal file handles XX and YY whether they have same parent
   and file name.  */

static int internal_dentry_eq_name(const void *xx, const void *yy)
{
	const struct internal_dentry_def *x =
		(const struct internal_dentry_def *)xx;
	const struct internal_dentry_def *y =
		(const struct internal_dentry_def *)yy;

	return (x->parent == y->parent
			&& x->name.len == y->name.len
			&& strcmp(x->name.str, y->name.str) == 0);
}

/*! Find the internal file handle or virtual directory for zfs_fh FH and set
   *VOLP, *DENTRYP and VDP according to it. If DELETE_VOLUME_P is true and the 
   volume should be deleted do not lookup the file handle and delete the
   volume if there are no file handles locked on it.  */

int32_t
zfs_fh_lookup(zfs_fh * fh, volume * volp, internal_dentry * dentryp,
			  virtual_dir * vdp, bool delete_volume_p)
{
	int32_t r;

	TRACE("");

	r = zfs_fh_lookup_nolock(fh, volp, dentryp, vdp, delete_volume_p);
	if (r == ZFS_OK)
		zfsd_mutex_unlock(&fh_mutex);

	RETURN_INT(r);
}

int32_t zfs_fh_lookup_virtual_dir(zfs_fh * fh, virtual_dir * vdp)
{
	hash_t hash = ZFS_FH_HASH(fh);
	virtual_dir vd = (virtual_dir) htab_find_with_hash(vd_htab, fh, hash);
	if (vd == NULL)
	{
		*vdp = NULL;
		RETURN_INT(ENOENT);
	}

	zfsd_mutex_lock(&vd->mutex);
#ifdef ENABLE_CHECKING
	if (vd->deleted > 0 && !vd->busy)
		zfsd_abort();
#endif
	*vdp = vd;
	RETURN_INT(ZFS_OK);
}


/*! Find the internal file handle or virtual directory for zfs_fh FH and set
   *VOLP, *DENTRYP and VDP according to it. This function is similar to
   FH_LOOKUP but the big locks must be locked. If DELETE_VOLUME_P is true and
   the volume should be deleted do not lookup the file handle and delete the
   volume if there are no file handles locked on it.  */

int32_t
zfs_fh_lookup_nolock(zfs_fh * fh, volume * volp, internal_dentry * dentryp,
					 virtual_dir * vdp, bool delete_volume_p)
{
	hash_t hash = ZFS_FH_HASH(fh);

	TRACE("");
#ifdef ENABLE_CHECKING
	if (fh->gen == 0)
		zfsd_abort();
#endif

	zfsd_mutex_lock(&fh_mutex);
	if (VIRTUAL_FH_P(*fh))
	{
		int rv = zfs_fh_lookup_virtual_dir(fh, vdp);
		if (rv != ZFS_OK) RETURN_INT(rv);

		if (volp)
		{
			zfsd_mutex_lock(&volume_mutex);
			if ((*vdp)->vol)
				zfsd_mutex_lock(&(*vdp)->vol->mutex);
			zfsd_mutex_unlock(&volume_mutex);
			*volp = (*vdp)->vol;
		}
		if (dentryp)
			*dentryp = NULL;

		RETURN_INT(ZFS_OK);
	}

	volume vol = NULL;
	internal_dentry dentry;

	if (volp)
	{
		vol = volume_lookup(fh->vid);
		if (!vol)
		{
			zfsd_mutex_unlock(&fh_mutex);
			RETURN_INT(ENOENT);
		}
		if (delete_volume_p && vol->delete_p)
		{
			if (vol->n_locked_fhs == 0)
				volume_delete(vol);
			else
				zfsd_mutex_unlock(&vol->mutex);
			zfsd_mutex_unlock(&fh_mutex);
			RETURN_INT(ENOENT);
		}
#ifdef ENABLE_CHECKING
		if (!delete_volume_p && vol->n_locked_fhs == 0)
			zfsd_abort();
#endif

		if (!vol->local_path.str && !volume_master_connected(vol))
		{
			zfsd_mutex_unlock(&vol->mutex);
			zfsd_mutex_unlock(&fh_mutex);
			RETURN_INT(ESTALE);
		}
	}

	dentry = (internal_dentry) htab_find_with_hash(dentry_htab, fh, hash);
	if (!dentry)
	{
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(ZFS_STALE);
	}

	acquire_dentry(dentry);

	if (volp)
	{
		if (CONFLICT_DIR_P(*fh) && !volume_master_connected(vol))
		{
			cancel_conflict(vol, dentry);
			RETURN_INT(ESTALE);
		}

		*volp = vol;
	}
	*dentryp = dentry;
	if (vdp)
		*vdp = NULL;

	RETURN_INT(ZFS_OK);
}

/*! Lock DENTRY and update time of last use.  */

void acquire_dentry(internal_dentry dentry)
{
	TRACE("%p", (void *)dentry);

	zfsd_mutex_lock(&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
	if (dentry->deleted)
		zfsd_abort();
#endif
	dentry_update_cleanup_node(dentry);

	RETURN_VOID;
}

/*! Update time of last use of DENTRY and unlock it.  */

void release_dentry(internal_dentry dentry)
{
	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	dentry_update_cleanup_node(dentry);
	zfsd_mutex_unlock(&dentry->fh->mutex);

	RETURN_VOID;
}

/*! Return virtual directory for file handle FH.  */

virtual_dir vd_lookup(zfs_fh * fh)
{
	virtual_dir vd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);

	vd = (virtual_dir) htab_find_with_hash(vd_htab, fh, ZFS_FH_HASH(fh));
	if (vd)
	{
		zfsd_mutex_lock(&vd->mutex);
#ifdef ENABLE_CHECKING
		if (vd->deleted > 0 && !vd->busy)
			zfsd_abort();
#endif
	}

	RETURN_PTR(vd);
}

virtual_dir
vd_lookup_name_dirstamp(virtual_dir parent, string * name,
						ATTRIBUTE_UNUSED_VERSIONS time_t * dirstamp)
{
	virtual_dir vd;
	struct virtual_dir_def tmp_vd;
	string verdir = { 0, NULL };

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&parent->mutex);

#ifdef ENABLE_VERSIONS
	if (zfs_config.versions.versioning && dirstamp)
	{
		int orgnamelen = 0;
		int32_t r;

		// version specified?
		r = version_get_filename_stamp(name->str, dirstamp, &orgnamelen);

		if (orgnamelen)
		{
			verdir.str = xstrdup(name->str);
			verdir.str[orgnamelen] = '\0';
			verdir.len = orgnamelen;
		}
	}
#endif

	tmp_vd.parent = parent;
	tmp_vd.name = verdir.str ? verdir : *name;

	vd = (virtual_dir) htab_find(vd_htab_name, &tmp_vd);
	if (vd)
	{
		zfsd_mutex_lock(&vd->mutex);
#ifdef ENABLE_CHECKING
		if (vd->deleted > 0 && !vd->busy)
			zfsd_abort();
#endif
	}

#ifdef ENABLE_VERSIONS
	if (verdir.str)
		free(verdir.str);
#endif

	RETURN_PTR(vd);
}

/*! Return the virtual directory for NAME in virtual directory PARENT.  */

virtual_dir vd_lookup_name(virtual_dir parent, string * name)
{
	return (vd_lookup_name_dirstamp(parent, name, NULL));
}

/*! Return the internal dentry for file handle FH.  */

internal_dentry dentry_lookup(zfs_fh * fh)
{
	internal_dentry dentry;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);

#ifdef ENABLE_CHECKING
	if (fh->gen == 0)
		zfsd_abort();
#endif

	dentry = (internal_dentry) htab_find_with_hash(dentry_htab, fh,
												   ZFS_FH_HASH(fh));
	if (dentry)
		acquire_dentry(dentry);

	RETURN_PTR(dentry);
}

/*! Lookup the internal dentry by name but do not lock it. \param vol Volume
   whose root is returned if parent == NULL. \param parent Directory in which
   the dentry is being looked up. \param name Name of the dentry.  */

static internal_dentry
dentry_lookup_name_nolock(volume vol, internal_dentry parent, string * name)
{
	struct internal_dentry_def tmp;
	internal_dentry dentry;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
#ifdef ENABLE_CHECKING
	if (!parent && !vol)
		zfsd_abort();
#endif

	if (parent)
	{
		tmp.parent = parent;
		tmp.name = *name;

		dentry = (internal_dentry) htab_find(dentry_htab_name, &tmp);
	}
	else
		dentry = vol->root_dentry;

#ifdef ENABLE_CHECKING
	if (dentry && dentry->parent != parent)
		zfsd_abort();
#endif

	RETURN_PTR(dentry);
}

/*! Lookup the internal dentry by name and lock it. \param vol Volume whose
   root is returned if parent == NULL. \param parent Directory in which the
   dentry is being looked up. \param name Name of the dentry.  */

internal_dentry
dentry_lookup_name(volume vol, internal_dentry parent, string * name)
{
	internal_dentry dentry;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
#ifdef ENABLE_CHECKING
	if (parent)
		CHECK_MUTEX_LOCKED(&parent->fh->mutex);
	else if (vol)
		CHECK_MUTEX_LOCKED(&vol->mutex);
	else
		zfsd_abort();
#endif

	dentry = dentry_lookup_name_nolock(vol, parent, name);
	if (dentry)
		acquire_dentry(dentry);

	RETURN_PTR(dentry);
}

/*! Return the internal dentry for PATH from directory START or from the
   volume root of volume VOL if START is NULL.  */

internal_dentry
dentry_lookup_path(volume vol, internal_dentry start, string * path)
{
	internal_dentry dentry;
	string name;
	char *str;

	TRACE("%s", path->str);
	CHECK_MUTEX_LOCKED(&fh_mutex);
#ifdef ENABLE_CHECKING
	if (start)
		CHECK_MUTEX_LOCKED(&start->fh->mutex);
	else if (vol)
		CHECK_MUTEX_LOCKED(&vol->mutex);
	else
		zfsd_abort();
#endif

	if (!start)
	{
		start = vol->root_dentry;
		if (!start)
			RETURN_PTR(NULL);
	}
	else
		release_dentry(start);

	if (CONFLICT_DIR_P(start->fh->local_fh))
	{
		dentry = dentry_lookup_name_nolock(vol, start, &this_node->name);
		if (!dentry)
			RETURN_PTR(NULL);

		start = dentry;
	}

	dentry = NULL;
	str = path->str;
	while (*str)
	{
		while (*str == '/')
			str++;

		name.str = str;
		while (*str != 0 && *str != '/')
			str++;
		if (*str == '/')
			*str++ = 0;
		name.len = strlen(name.str);

		dentry = dentry_lookup_name_nolock(vol, start, &name);
		if (!dentry)
			RETURN_PTR(NULL);

		start = dentry;
		if (CONFLICT_DIR_P(start->fh->local_fh))
		{
			dentry = dentry_lookup_name_nolock(vol, start, &this_node->name);
			if (!dentry)
				RETURN_PTR(NULL);

			start = dentry;
		}
	}

	acquire_dentry(dentry);
	RETURN_PTR(dentry);
}

/*! Return the internal dentry for LOCAL_PATH on volume VOL.  */

internal_dentry dentry_lookup_local_path(volume vol, string * local_path)
{
	string relative_path;
	internal_dentry dentry;

	TRACE("%s", local_path->str);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	local_path_to_relative_path(&relative_path, vol, local_path);

	dentry = dentry_lookup_path(vol, NULL, &relative_path);

	free(relative_path.str);
	RETURN_PTR(dentry);
}

/*! Lock dentry *DENTRYP on volume *VOLP to level LEVEL. Store the local ZFS
   file handle to TMP_FH.  */

int32_t
internal_dentry_lock(unsigned int level, volume * volp,
					 internal_dentry * dentryp, zfs_fh * tmp_fh)
{
	int32_t r;
	bool wait_for_locked;
	unsigned int id;

	TRACE("%p", (void *)*dentryp);
#ifdef ENABLE_CHECKING
	if (volp == NULL)
		zfsd_abort();
	if (dentryp == NULL)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
	if (level > LEVEL_EXCLUSIVE)
		zfsd_abort();
#endif

	message(LOG_LOCK, FACILITY_DATA | FACILITY_THREADING,
			"FH %p LOCK %u, by %"PTRid" at %s:%d\n", (void *)(*dentryp)->fh, level,
			PTRid_conversion pthread_self(), __FILE__, __LINE__);

	*tmp_fh = (*dentryp)->fh->local_fh;
	id = (*dentryp)->fh->id2assign++;
	wait_for_locked = internal_fh_should_wait_for_locked((*dentryp)->fh, level);
	if (wait_for_locked)
	{
		zfsd_mutex_unlock(&(*volp)->mutex);

		do
		{
			zfsd_cond_wait(&(*dentryp)->fh->cond, &(*dentryp)->fh->mutex);

			if ((*dentryp)->deleted) break;
			//if ((*dentryp)->fh->id2run == id) break;
		}
		while(internal_fh_should_wait_for_locked((*dentryp)->fh, level));

		zfsd_mutex_unlock(&(*dentryp)->fh->mutex);

		r = zfs_fh_lookup_nolock(tmp_fh, volp, dentryp, NULL, true);
		if (r != ZFS_OK)
			RETURN_INT(r);
	}

	message(LOG_LOCK, FACILITY_DATA | FACILITY_THREADING,
			"FH %p LOCKED %u, by %"PTRid" at %s:%d\n", (void *)(*dentryp)->fh,
			level, PTRid_conversion pthread_self(), __FILE__, __LINE__);

	(*dentryp)->fh->level = level;
	(*dentryp)->fh->users++;
	(*dentryp)->users++;
	(*volp)->n_locked_fhs++;
	set_owned(*dentryp, level);

	(*dentryp)->fh->id2run++;
	if (level != LEVEL_EXCLUSIVE)
		zfsd_cond_broadcast(&(*dentryp)->fh->cond);

	if (!wait_for_locked)
	{
		release_dentry(*dentryp);
		zfsd_mutex_unlock(&(*volp)->mutex);

		r = zfs_fh_lookup_nolock(tmp_fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
		if (r != ZFS_OK)
			zfsd_abort();
#endif
	}

	RETURN_INT(ZFS_OK);
}

/*! Unlock dentry DENTRY.  */

void internal_dentry_unlock(volume vol, internal_dentry dentry)
{
	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
	if (dentry->fh->level == LEVEL_UNLOCKED)
		zfsd_abort();
	if (dentry->users == 0)
		zfsd_abort();
	if (dentry->fh->users == 0)
		zfsd_abort();
#endif

	message(LOG_LOCK, FACILITY_DATA | FACILITY_THREADING,
			"FH %p UNLOCK, by %"PTRid" at %s:%d\n", (void *)dentry->fh,
			PTRid_conversion pthread_self(), __FILE__, __LINE__);

	vol->n_locked_fhs--;
	zfsd_mutex_unlock(&vol->mutex);
	dentry->fh->users--;
	dentry->users--;
	clear_owned(dentry);
	if (dentry->fh->users == 0)
	{
		dentry->fh->level = LEVEL_UNLOCKED;
		destroy_unused_capabilities(dentry->fh);
		if (dentry->deleted)
		{
			zfsd_abort();
			internal_dentry_destroy(dentry, true, true,
									dentry->parent == NULL);
		}
		else
		{
			zfsd_cond_broadcast(&dentry->fh->cond);
			release_dentry(dentry);
		}
	}
	else
		release_dentry(dentry);
	zfsd_mutex_unlock(&fh_mutex);

	RETURN_VOID;
}

/*! Lock 2 dentries on volume *VOLP, lock *DENTRY1P to level LEVEL1 and
   *DENTRY2P to level LEVEL2.  Use TMP_FH1 and TMP_FH2 to lookup them.  */

int32_t
internal_dentry_lock2(unsigned int level1, unsigned int level2, volume * volp,
					  internal_dentry * dentry1p, internal_dentry * dentry2p,
					  zfs_fh * tmp_fh1, zfs_fh * tmp_fh2)
{
	int32_t r, r2;

	TRACE("%p %p", (void *)*dentry1p, (void *)*dentry2p);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*dentry1p)->fh->mutex);
	CHECK_MUTEX_LOCKED(&(*dentry2p)->fh->mutex);

	if (tmp_fh1->ino == tmp_fh2->ino && tmp_fh1->dev == tmp_fh2->dev)
	{
		r = internal_dentry_lock((level1 > level2 ? level1 : level2),
								 volp, dentry1p, tmp_fh1);
		if (r != ZFS_OK)
			RETURN_INT(r);

		*dentry2p = *dentry1p;
		RETURN_INT(ZFS_OK);
	}

	if (tmp_fh1->ino < tmp_fh2->ino
		|| (tmp_fh1->ino == tmp_fh2->ino && tmp_fh1->dev < tmp_fh2->dev))
	{
		release_dentry(*dentry2p);

		r = internal_dentry_lock(level1, volp, dentry1p, tmp_fh1);
		if (r != ZFS_OK)
			RETURN_INT(r);

		release_dentry(*dentry1p);
		zfsd_mutex_unlock(&(*volp)->mutex);
		zfsd_mutex_unlock(&fh_mutex);

		r = zfs_fh_lookup(tmp_fh2, volp, dentry2p, NULL, true);
		if (r != ZFS_OK)
			goto out1;

		r = internal_dentry_lock(level2, volp, dentry2p, tmp_fh2);
		if (r != ZFS_OK)
		{
		  out1:
			r2 = zfs_fh_lookup_nolock(tmp_fh1, volp, dentry1p, NULL, false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
				zfsd_abort();
#endif

			internal_dentry_unlock(*volp, *dentry1p);
			RETURN_INT(r);
		}

		release_dentry(*dentry2p);
		zfsd_mutex_unlock(&(*volp)->mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}
	else						/* if (tmp_fh1->ino > tmp_fh2->ino ||
								   (tmp_fh1->ino == tmp_fh2->ino &&
								   tmp_fh1->dev > tmp_fh2->dev) */
	{
		release_dentry(*dentry1p);

		r = internal_dentry_lock(level2, volp, dentry2p, tmp_fh2);
		if (r != ZFS_OK)
			RETURN_INT(r);

		release_dentry(*dentry2p);
		zfsd_mutex_unlock(&(*volp)->mutex);
		zfsd_mutex_unlock(&fh_mutex);

		r = zfs_fh_lookup(tmp_fh1, volp, dentry1p, NULL, true);
		if (r != ZFS_OK)
			goto out2;

		r = internal_dentry_lock(level1, volp, dentry1p, tmp_fh1);
		if (r != ZFS_OK)
		{
		  out2:
			r2 = zfs_fh_lookup_nolock(tmp_fh2, volp, dentry2p, NULL, false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
				zfsd_abort();
#endif

			internal_dentry_unlock(*volp, *dentry2p);
			RETURN_INT(r);
		}

		release_dentry(*dentry1p);
		zfsd_mutex_unlock(&(*volp)->mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}

	/* Lookup dentries again.  */
	r2 = zfs_fh_lookup_nolock(tmp_fh1, volp, dentry1p, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	*dentry2p = dentry_lookup(tmp_fh2);
#ifdef ENABLE_CHECKING
	if (!*dentry2p)
		zfsd_abort();
#endif

	RETURN_INT(ZFS_OK);
}

/*! Set master file handle of file handle FH on volume VOL to MASTER_FH.  */

bool set_master_fh(volume vol, internal_fh fh, zfs_fh * master_fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	if (zfs_fh_undefined(*master_fh))
		RETURN_BOOL(true);

	if (INTERNAL_FH_HAS_LOCAL_PATH(fh))
		RETURN_BOOL(set_metadata_master_fh(vol, fh, master_fh));

	fh->meta.master_fh = *master_fh;
	RETURN_BOOL(true);
}

/*! Clear metadata in file handle FH.  */

static void clear_meta(internal_fh fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&fh->mutex);

	memset(&fh->meta, 0, offsetof(metadata, master_fh));
	zfs_fh_undefine(fh->meta.master_fh);
	RETURN_VOID;
}

/*! Create a new internal file handle on volume VOL with local file handle
   LOCAL_FH, remote file handle MASTER_FH, attributes ATTR, lock it to level
   LEVEL and store it to hash tables.  */

static internal_fh
internal_fh_create(zfs_fh * local_fh, zfs_fh * master_fh, fattr * attr,
				   metadata * meta, volume vol, unsigned int level)
{
	internal_fh fh;
	void **slot;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	fh = (internal_fh) pool_alloc(fh_pool);
	fh->local_fh = *local_fh;
	fh->attr = *attr;
	fh->cap = NULL;
	fh->ndentries = 0;
	fh->updated = NULL;
	fh->modified = NULL;
	fh->interval_tree_users = 0;
	fh->journal = NULL;
	fh->level = level;
	fh->users = 0;
	fh->id2assign = 0;
	fh->id2run = 0;
	fh->fd = -1;
	fh->generation = 0;
	fh->flags = 0;
	fh->reintegrating_sid = 0;
	fh->reintegrating_generation = 0;
#ifdef ENABLE_VERSIONS
	fh->version_fd = -1;
	fh->version_path = NULL;
	fh->versioned = NULL;
	UNMARK_FILE_TRUNCATED(fh);
	fh->marked_size = -1;
	fh->version_interval_tree_users = 0;
	fh->version_list = NULL;
	fh->version_list_length = 0;
#endif

	message(LOG_DEBUG, FACILITY_DATA, "FH %p CREATED, by %"PTRid"\n", (void *)fh,
			PTRid_conversion pthread_self());

	if (fh->attr.type == FT_DIR)
		varray_create(&fh->subdentries, sizeof(internal_dentry), 16);

	zfsd_mutex_init(&fh->mutex);
	zfsd_mutex_lock(&fh->mutex);
	zfsd_cond_init(&fh->cond);

	if (level != LEVEL_UNLOCKED)
	{
#ifdef ENABLE_CHECKING
		if (level != LEVEL_SHARED && level != LEVEL_EXCLUSIVE)
			zfsd_abort();
#endif
		fh->users++;
		vol->n_locked_fhs++;
	}

	slot = htab_find_slot_with_hash(fh_htab, &fh->local_fh,
									INTERNAL_FH_HASH(fh), INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = fh;

	if (INTERNAL_FH_HAS_LOCAL_PATH(fh))
	{
#ifdef ENABLE_CHECKING
		if (local_fh->dev != meta->dev
			|| local_fh->ino != meta->ino || local_fh->gen != meta->gen)
			zfsd_abort();
		if (meta->slot_status != VALID_SLOT)
			zfsd_abort();
#endif
		fh->meta = *meta;
		set_attr_version(&fh->attr, &fh->meta);
		attr->version = fh->attr.version;

		if (fh->attr.type == FT_DIR)
		{
			fh->journal = journal_create(5, &fh->mutex);
			if (!read_journal(vol, &fh->local_fh, fh->journal))
				MARK_VOLUME_DELETE(vol);
		}
	}
	else
		clear_meta(fh);

	if (!vol->delete_p && !set_master_fh(vol, fh, master_fh))
	{
		MARK_VOLUME_DELETE(vol);
		clear_meta(fh);
	}

	RETURN_PTR(fh);
}

/*! Destroy almost everything of the internal file handle FH except mutex and 
   file handle itself.  */

static void internal_fh_destroy_stage1(internal_fh fh)
{
	void **slot;
	internal_cap cap, next;

	TRACE("%p", (void *)fh);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

#ifdef ENABLE_CHECKING
	if (fh->ndentries != 0)
		zfsd_abort();
#endif

	message(LOG_DEBUG, FACILITY_DATA, "FH %p DESTROY, by %"PTRid"\n", (void *)fh,
			PTRid_conversion pthread_self());

	/* Destroy capabilities associated with file handle.  */
	for (cap = fh->cap; cap; cap = next)
	{
		next = cap->next;
		cap->busy = 1;
		put_capability(cap, fh, NULL);
	}

	if (fh->attr.type == FT_DIR)
		varray_destroy(&fh->subdentries);

	if (fh->journal)
	{
		close_journal_file(fh->journal);
		journal_destroy(fh->journal);
	}

#ifdef ENABLE_VERSIONS
	if (fh->version_list)
	{
		unsigned int i;
		for (i = 0; i < fh->version_list_length; i++)
			CLEAR_VERSION_ITEM(fh->version_list[i]);
		free(fh->version_list);
	}

	if (fh->version_path)
		free(fh->version_path);

	if (fh->versioned)
		interval_tree_destroy(fh->versioned);
#endif

	slot = htab_find_slot_with_hash(fh_htab, &fh->local_fh,
									INTERNAL_FH_HASH(fh), NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif
	htab_clear_slot(fh_htab, slot);

	RETURN_VOID;
}

/*! Destroy the rest of the internal file handle FH, i.e. the mutex and file
   handle itself.  */

static void internal_fh_destroy_stage2(internal_fh fh)
{
	TRACE("%p", (void *)fh);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&fh->mutex);

	message(LOG_DEBUG, FACILITY_DATA, "FH %p DESTROYED, by %"PTRid"\n", (void *)fh,
			PTRid_conversion pthread_self());

	zfsd_mutex_unlock(&fh->mutex);
	zfsd_mutex_destroy(&fh->mutex);
	pool_free(fh_pool, fh);

	RETURN_VOID;
}

/*! should wait for other thread termination before locking internal_fh ? */

bool internal_fh_should_wait_for_locked(const internal_fh fh, int new_level)
{
#ifdef ENABLE_CHECKING
	CHECK_MUTEX_LOCKED(&fh->mutex);
#endif
	switch (fh->level)
	{
		case LEVEL_UNLOCKED:
			return false;
		case LEVEL_SHARED:
			if (new_level == LEVEL_SHARED)
			// shared share is probably broken on others part of zlomekFS code
			//	return false;
				return true;
			else
				return true;
		case LEVEL_EXCLUSIVE:
			return true;
		default:
			zfsd_abort();
			return true;
	}

	zfsd_abort();
	return true;
}

void for_each_internal_fh(void(*visit)(const internal_fh, void *), void * data)
{
	void ** slot;
	zfsd_mutex_lock(&fh_mutex);
	HTAB_FOR_EACH_SLOT(fh_htab, slot)
	{
		visit((internal_fh) * slot, data);
	}
	zfsd_mutex_unlock(&fh_mutex);
}

/*! Print the contents of hash table HTAB to file F.  */

void print_fh_htab(FILE * f)
{
	void **slot;

	HTAB_FOR_EACH_SLOT(fh_htab, slot)
	{
		internal_fh fh = (internal_fh) * slot;

		fprintf(f, "[%u,%u,%u,%u,%u] ", fh->local_fh.sid, fh->local_fh.vid,
				fh->local_fh.dev, fh->local_fh.ino, fh->local_fh.gen);
		fprintf(f, "[%u,%u,%u,%u,%u] ", fh->meta.master_fh.sid,
				fh->meta.master_fh.vid, fh->meta.master_fh.dev,
				fh->meta.master_fh.ino, fh->meta.master_fh.gen);
		fprintf(f, "L%d ", fh->level);
		fprintf(f, "\n");
	}
}

/*! Print the contents of hash table of filehandles HTAB to STDERR.  */

void debug_fh_htab(void)
{
	print_fh_htab(stderr);
}

/*! Print subdentries of dentry DENTRY to file F.  */

void print_subdentries(FILE * f, internal_dentry dentry)
{
	unsigned int i;
	internal_dentry subdentry;

	if (dentry->fh->attr.type != FT_DIR)
		return;

	for (i = 0; i < VARRAY_USED(dentry->fh->subdentries); i++)
	{
		subdentry = VARRAY_ACCESS(dentry->fh->subdentries, i, internal_dentry);

		fprintf(f,
				"%s [%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
				"]\n", subdentry->name.str, subdentry->fh->local_fh.sid,
				subdentry->fh->local_fh.vid, subdentry->fh->local_fh.dev,
				subdentry->fh->local_fh.ino, subdentry->fh->local_fh.gen);
	}
}

/*! Print subdentries of dentry DENTRY to STDERR.  */

void debug_subdentries(internal_dentry dentry)
{
	print_subdentries(stderr, dentry);
}

/*! Add DENTRY to list of dentries of PARENT.  */

static void
internal_dentry_add_to_dir(internal_dentry parent, internal_dentry dentry)
{
	void **slot;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (!parent)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&parent->fh->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

#ifdef ENABLE_CHECKING
	if (dentry->parent)
		zfsd_abort();
#endif
	dentry->parent = parent;

	dentry->dentry_index = VARRAY_USED(parent->fh->subdentries);
	VARRAY_PUSH(parent->fh->subdentries, dentry, internal_dentry);
	dentry_update_cleanup_node(parent);
	dentry_update_cleanup_node(dentry);

	slot = htab_find_slot(dentry_htab_name, dentry, INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = dentry;
}

/*! Delete DENTRY from the list of dentries of its parent.  */

static void internal_dentry_del_from_dir(internal_dentry dentry)
{
	internal_dentry top;
	void **slot;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	if (!dentry->parent)
		RETURN_VOID;

	CHECK_MUTEX_LOCKED(&dentry->parent->fh->mutex);

	top = VARRAY_TOP(dentry->parent->fh->subdentries, internal_dentry);
	VARRAY_ACCESS(dentry->parent->fh->subdentries, dentry->dentry_index,
				  internal_dentry) = top;
	VARRAY_POP(dentry->parent->fh->subdentries);
	top->dentry_index = dentry->dentry_index;

	slot = htab_find_slot(dentry_htab_name, dentry, NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif
	htab_clear_slot(dentry_htab_name, slot);

	dentry_update_cleanup_node(dentry->parent);
	dentry->parent = NULL;
	RETURN_VOID;
}

/*! Create a new internal dentry NAME in directory PARENT on volume VOL and
   internal file handle for local file handle LOCAL_FH and master file handle
   MASTER_FH with attributes ATTR and store it to hash tables. Lock the newly
   created file handle to level LEVEL.  */

static internal_dentry
internal_dentry_create(zfs_fh * local_fh, zfs_fh * master_fh, volume vol,
					   internal_dentry parent, string * name, fattr * attr,
					   metadata * meta, unsigned int level)
{
	internal_dentry dentry;
	internal_fh fh;
	void **slot;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (parent)
	{
		CHECK_MUTEX_LOCKED(&parent->fh->mutex);
	}
#endif

	dentry = (internal_dentry) pool_alloc(dentry_pool);
	dentry->parent = NULL;
	xstringdup(&dentry->name, name);
	dentry->next = dentry;
	dentry->prev = dentry;
	dentry->last_use = time(NULL);
	dentry->heap_node = NULL;
	dentry->users = 0;
	dentry->deleted = false;
#ifdef ENABLE_VERSIONS
	dentry->version_file = false;
	dentry->new_file = false;
	dentry->dirstamp = 0;
	dentry->dirhtab = NULL;
	dentry->version_dirty = false;
	dentry->version_dentry = NULL;
	dentry->version_interval_dentry = NULL;
#endif

	/* Find the internal file handle in hash table, create it if it does not
	   exist.  */
	if (CONFLICT_DIR_P(*local_fh))
	{
		do
		{
			vol->last_conflict_ino++;
			if (vol->last_conflict_ino == 0)
				vol->last_conflict_ino++;

			local_fh->ino = vol->last_conflict_ino;
			slot = htab_find_slot_with_hash(fh_htab, local_fh,
											ZFS_FH_HASH(local_fh), INSERT);
		}
		while (*slot);
	}
	else
	{
		slot = htab_find_slot_with_hash(fh_htab, local_fh,
										ZFS_FH_HASH(local_fh), INSERT);
	}
	if (!*slot)
	{
		fh = internal_fh_create(local_fh, master_fh, attr, meta, vol, level);
		if (level != LEVEL_UNLOCKED)
		{
#ifdef ENABLE_CHECKING
			if (level != LEVEL_SHARED && level != LEVEL_EXCLUSIVE)
				zfsd_abort();
#endif
			dentry->users++;
			set_owned(dentry, level);
		}
	}
	else
	{
		fh = (internal_fh) * slot;
		zfsd_mutex_lock(&fh->mutex);
		fh->attr = *attr;
	}

	dentry->fh = fh;
	fh->ndentries++;

	if (parent)
	{
		dentry_update_cleanup_node(dentry);
		internal_dentry_add_to_dir(parent, dentry);

		if (INTERNAL_FH_HAS_LOCAL_PATH(fh))
		{
			if (!metadata_hardlink_insert(vol, &fh->local_fh, meta,
										  parent->fh->local_fh.dev,
										  parent->fh->local_fh.ino, name))
			{
				MARK_VOLUME_DELETE(vol);
			}
		}
	}
	else
		vol->root_dentry = dentry;

	slot = htab_find_slot_with_hash(dentry_htab, &fh->local_fh,
									INTERNAL_DENTRY_HASH(dentry), INSERT);
	if (*slot)
	{
		internal_dentry old = (internal_dentry) * slot;

		dentry->next = old->next;
		dentry->prev = old;
		old->next->prev = dentry;
		old->next = dentry;

		if (parent)
		{
			/* Lower the fibheap keys if they are FIBHEAPKEY_MAX.  */
			if (dentry->heap_node && dentry->heap_node->key == FIBHEAPKEY_MAX)
				dentry_update_cleanup_node(dentry);
			for (old = dentry->next; old != dentry; old = old->next)
				if (old->heap_node && old->heap_node->key == FIBHEAPKEY_MAX)
					dentry_update_cleanup_node(old);
		}
	}
	*slot = dentry;

#ifdef ENABLE_VERSIONS
	if (zfs_config.versions.versioning && strchr(name->str, VERSION_NAME_SPECIFIER_C))
		dentry->version_file = true;
#endif

	RETURN_PTR(dentry);
}

internal_dentry internal_dentry_create_ns(zfs_fh * local_fh,
										  zfs_fh * master_fh, volume vol,
										  internal_dentry parent,
										  string * name, fattr * attr,
										  metadata * meta, unsigned int level)
{
	return internal_dentry_create(local_fh, master_fh, vol, parent, name, attr,
								  meta, level);
}


/*! Return dentry for file NAME in directory DIR on volume VOL. If it does
   not exist create it.  Update its local file handle to LOCAL_FH, master file 
   handle to MASTER_FH and attributes to ATTR.  */

internal_dentry
get_dentry(zfs_fh * local_fh, zfs_fh * master_fh, volume vol,
		   internal_dentry dir, string * name, fattr * attr, metadata * meta)
{
	internal_dentry dentry, subdentry;
	zfs_fh tmp;
	int32_t r;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (dir)
	{
		CHECK_MUTEX_LOCKED(&dir->fh->mutex);
		if (dir->fh->level == LEVEL_UNLOCKED)
			zfsd_abort();
	}
#endif

	dentry = dentry_lookup_name(vol, dir, name);
	if (dentry && CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		if (dir)
		{
			tmp = dir->fh->local_fh;
			release_dentry(dir);
		}
		else
			tmp.vid = vol->id;

		if (volume_master_connected(vol))
		{
			subdentry = add_file_to_conflict_dir(vol, dentry, true, local_fh,
												 attr, meta);
			if (!try_resolve_conflict(vol, dentry))
			{
				/* DIR was locked so it can't have been deleted.  */
				if (dir)
					acquire_dentry(dir);
				release_dentry(dentry);
				/* We did not unlock fh_mutex so SUBDENTRY is still valid.  */
				acquire_dentry(subdentry);
				RETURN_PTR(subdentry);
			}
			zfsd_mutex_unlock(&fh_mutex);
		}
		else
		{
			cancel_conflict(vol, dentry);
		}

		if (dir)
		{
			r = zfs_fh_lookup_nolock(&tmp, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
			if (r != ZFS_OK)
				zfsd_abort();
#endif
		}
		else
		{
			zfsd_mutex_lock(&fh_mutex);
			vol = volume_lookup(tmp.vid);
#ifdef ENABLE_CHECKING
			if (!vol)
				zfsd_abort();
#endif
		}

		dentry = dentry_lookup_name(vol, dir, name);
#ifdef ENABLE_CHECKING
		if (dentry && CONFLICT_DIR_P(dentry->fh->local_fh))
			zfsd_abort();
#endif
	}

	if (dentry)
	{
		CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

		if (!ZFS_FH_EQ(dentry->fh->local_fh, *local_fh)
			|| (!ZFS_FH_EQ(dentry->fh->meta.master_fh, *master_fh)
				&& !zfs_fh_undefined(dentry->fh->meta.master_fh)
				&& !zfs_fh_undefined(*master_fh)))
		{
			unsigned int level;

			if (dir)
			{
#ifdef ENABLE_CHECKING
				if (dir->fh->level == LEVEL_UNLOCKED
					&& dentry->fh->level == LEVEL_UNLOCKED)
					zfsd_abort();
#endif
				tmp = dir->fh->local_fh;
				release_dentry(dir);
			}
			else
				tmp.vid = vol->id;
			zfsd_mutex_unlock(&vol->mutex);

			level = get_level(dentry);
			internal_dentry_destroy(dentry, true, true,
									dentry->parent == NULL);

			if (dir)
			{
				zfsd_mutex_unlock(&fh_mutex);
				r = zfs_fh_lookup_nolock(&tmp, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
				if (r != ZFS_OK)
					zfsd_abort();
#endif
			}
			else
			{
				vol = volume_lookup(tmp.vid);
#ifdef ENABLE_CHECKING
				if (!vol)
					zfsd_abort();
#endif
			}
			dentry =
				internal_dentry_create(local_fh, master_fh, vol, dir, name,
									   attr, meta, level);
		}
		else
		{
			if (zfs_fh_undefined(dentry->fh->meta.master_fh))
				set_master_fh(vol, dentry->fh, master_fh);

			if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
				set_attr_version(attr, &dentry->fh->meta);
			dentry->fh->attr = *attr;
		}
	}
	else
		dentry = internal_dentry_create(local_fh, master_fh, vol, dir, name,
										attr, meta, LEVEL_UNLOCKED);

	RETURN_PTR(dentry);
}

/*! Destroy dentry NAME in directory DIR (whose file handle is DIR_FH) on
   volume VOL.  */

void
delete_dentry(volume * volp, internal_dentry * dirp, string * name,
			  zfs_fh * dir_fh)
{
	internal_dentry dentry;
	int32_t r2;

	TRACE("%p", (void *)*dirp);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*dirp)->fh->mutex);
#ifdef ENABLE_CHECKING
	if ((*dirp)->fh->level == LEVEL_UNLOCKED)
		zfsd_abort();
#endif

	dentry = dentry_lookup_name(NULL, *dirp, name);
	if (dentry)
	{
		if (CONFLICT_DIR_P(dentry->fh->local_fh))
		{
			zfs_fh tmp_fh;
			internal_dentry subdentry;

			release_dentry(*dirp);
			zfsd_mutex_unlock(&(*volp)->mutex);
			subdentry = conflict_local_dentry(dentry);
#ifdef ENABLE_CHECKING
			if (!subdentry)
				zfsd_abort();
#endif

			tmp_fh = dentry->fh->local_fh;
			release_dentry(dentry);

			internal_dentry_destroy(subdentry, true, true,
									subdentry->parent == NULL);

			dentry = dentry_lookup(&tmp_fh);
			*volp = volume_lookup(tmp_fh.vid);
			if (!try_resolve_conflict(*volp, dentry))
			{
				release_dentry(dentry);
				zfsd_mutex_unlock(&(*volp)->mutex);
			}
		}
		else
		{
			release_dentry(*dirp);
			zfsd_mutex_unlock(&(*volp)->mutex);

			internal_dentry_destroy(dentry, true, true,
									dentry->parent == NULL);
		}

		zfsd_mutex_unlock(&fh_mutex);
		r2 = zfs_fh_lookup_nolock(dir_fh, volp, dirp, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif
	}
}

/*! Create a new internal dentry NAME in directory PARENT on volume VOL for
   file DENTRY.  */

internal_dentry
internal_dentry_link(internal_dentry orig,
					 internal_dentry parent, string * name)
{
	internal_dentry dentry, old;
	void **slot;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (!parent)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&orig->fh->mutex);
	CHECK_MUTEX_LOCKED(&parent->fh->mutex);

#ifdef ENABLE_CHECKING
	dentry = dentry_lookup_name(NULL, parent, name);
	if (dentry)
		zfsd_abort();
#endif

	dentry = (internal_dentry) pool_alloc(dentry_pool);
	dentry->parent = NULL;
	xstringdup(&dentry->name, name);
	dentry->fh = orig->fh;
	orig->fh->ndentries++;
	dentry->next = dentry;
	dentry->prev = dentry;
	dentry->last_use = time(NULL);
	dentry->heap_node = NULL;
	dentry->users = 0;
	dentry->deleted = false;
#ifdef ENABLE_VERSIONS
	dentry->version_file = false;
#endif

	dentry_update_cleanup_node(dentry);
	internal_dentry_add_to_dir(parent, dentry);

	slot = htab_find_slot_with_hash(dentry_htab, &orig->fh->local_fh,
									INTERNAL_DENTRY_HASH(dentry), INSERT);
	if (*slot)
	{
		old = (internal_dentry) * slot;
		dentry->next = old->next;
		dentry->prev = old;
		old->next->prev = dentry;
		old->next = dentry;
	}
#ifdef ENABLE_CHECKING
	else
		zfsd_abort();
#endif

	RETURN_PTR(dentry);
}

/*! Move internal dentry for file FROM_NAME in *FROM_DIRP to be a subdentry
   of *TO_DIRP with name TO_NAME on volume *VOLP.  */

void
internal_dentry_move(internal_dentry * from_dirp, string * from_name,
					 internal_dentry * to_dirp, string * to_name,
					 volume * volp, zfs_fh * from_fh, zfs_fh * to_fh)
{
	zfs_fh tmp_fh;
	internal_dentry dentry;
#ifdef ENABLE_CHECKING
	internal_dentry tmp;
#endif

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*from_dirp)->fh->mutex);
	CHECK_MUTEX_LOCKED(&(*to_dirp)->fh->mutex);

	dentry = dentry_lookup_name(NULL, *from_dirp, from_name);
	if (!dentry)
		RETURN_VOID;

#ifdef ENABLE_CHECKING
	/* Check whether we are not moving DENTRY to its subtree.  */
	for (tmp = *to_dirp; tmp; tmp = tmp->parent)
		if (tmp == dentry)
			zfsd_abort();

	/* There should be no dentry in *TO_DIRP with name TO_NAME.  */
	tmp = dentry_lookup_name(NULL, *to_dirp, to_name);
	if (tmp)
		zfsd_abort();
#endif

	if (CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		internal_dentry conflict, parent;

		conflict = dentry;
		parent = conflict->parent;
		internal_dentry_del_from_dir(conflict);
		dentry = conflict_local_dentry(conflict);
#ifdef ENABLE_CHECKING
		if (!dentry)
			zfsd_abort();
#endif

		internal_dentry_del_from_dir(dentry);
		free(dentry->name.str);
		xstringdup(&dentry->name, to_name);
		internal_dentry_add_to_dir(*to_dirp, dentry);
		tmp_fh = dentry->fh->local_fh;
		release_dentry(dentry);

		release_dentry(*from_dirp);
		if (*to_dirp != *from_dirp)
			release_dentry(*to_dirp);
		zfsd_mutex_unlock(&(*volp)->mutex);

		internal_dentry_destroy(conflict, false, true, parent == NULL);

		*volp = volume_lookup(to_fh->vid);
		*to_dirp = dentry_lookup(to_fh);
		if (from_fh->ino != to_fh->ino)
			*from_dirp = dentry_lookup(from_fh);
		else
			*from_dirp = *to_dirp;

		fs_invalidate_fh(&tmp_fh);
	}
	else
	{
		internal_dentry_del_from_dir(dentry);
		free(dentry->name.str);
		xstringdup(&dentry->name, to_name);
		internal_dentry_add_to_dir(*to_dirp, dentry);
		fs_invalidate_dentry(dentry, dentry->parent == NULL);
	}
	RETURN_VOID;
}

/*! Destroy subdentries of dentry DENTRY.  Invalidate the dentries in kernel
   if INVALIDATE.  Return true if DENTRY still exists.  */

static bool
internal_dentry_destroy_subdentries(internal_dentry dentry, zfs_fh * tmp_fh,
									bool invalidate)
{
	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
	if (dentry->fh->attr.type != FT_DIR)
		zfsd_abort();
#endif

	while (VARRAY_USED(dentry->fh->subdentries) > 0)
	{
		internal_dentry subdentry;
		internal_dentry tmp1, tmp2;

		subdentry = VARRAY_TOP(dentry->fh->subdentries, internal_dentry);
		zfsd_mutex_lock(&subdentry->fh->mutex);
		zfsd_mutex_unlock(&dentry->fh->mutex);
		internal_dentry_destroy(subdentry, false, invalidate, false);

		tmp1 = dentry_lookup(tmp_fh);
		/* DENTRY could not be found, it is already deleted.  */
		if (tmp1 == NULL)
			RETURN_BOOL(false);

		tmp2 = tmp1;
		do
		{
			if (tmp2 == dentry)
				break;
			tmp2 = tmp2->next;
		}
		while (tmp2 != tmp1);

		/* DENTRY could not be found, it is already deleted.  */
		if (tmp2 != dentry)
			RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

/*! Destroy internal dentry. \param dentry Dentry which shall be destroyed.
   \param clear_volume_root Flag whether the volume root shall be cleared.
   \param invalidate Flag whether the dentry shall be invalidated. \param
   volume_root_p Flag whether the dentry was the volume root.  */

void
internal_dentry_destroy(internal_dentry dentry, bool clear_volume_root,
						bool invalidate, bool volume_root_p)
{
	zfs_fh tmp_fh;
	void **slot;

	TRACE("%p", (void *)dentry);
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	tmp_fh = dentry->fh->local_fh;

	if (dentry->fh->attr.type == FT_DIR)
	{
		/* Destroy subtree first.  */
		if (!internal_dentry_destroy_subdentries(dentry, &tmp_fh, invalidate))
			RETURN_VOID;
	}

#ifdef ENABLE_CHECKING
	if (dentry->fh->level != LEVEL_UNLOCKED && dentry->deleted)
		zfsd_abort();
#endif

	/* If we are holding the lock unlock it first.  */
	if (is_owned(dentry))
	{
		volume vol;

		message(LOG_DEBUG, FACILITY_DATA, "FH %p DELETE, by %"PTRid"\n",
				(void *)dentry->fh, PTRid_conversion pthread_self());

		vol = volume_lookup(tmp_fh.vid);
		vol->n_locked_fhs--;
		zfsd_mutex_unlock(&vol->mutex);

		dentry->fh->users--;
		dentry->users--;
		clear_owned(dentry);
		if (dentry->fh->users == 0)
			dentry->fh->level = LEVEL_UNLOCKED;
	}

	while (dentry->users > 0)
	{
#ifdef ENABLE_CHECKING
		internal_dentry tmp1, tmp2;
#endif
		internal_fh fh = dentry->fh;

		zfsd_mutex_unlock(&fh_mutex);

		/* FH can't be deleted while it is locked.  */
		/* Using two different mutexes with the same condition
		at the same time could lead to unpredictable serialization issues in your application. */
		zfsd_cond_wait(&fh->cond, &fh->mutex);
		zfsd_mutex_unlock(&fh->mutex);
		zfsd_mutex_lock(&fh_mutex);

#ifdef ENABLE_CHECKING
		tmp1 = dentry_lookup(&tmp_fh);
		tmp2 = tmp1;
		if (tmp1 == NULL)
			zfsd_abort();
		do
		{
			if (tmp2 == dentry)
				break;
			tmp2 = tmp2->next;
		}
		while (tmp2 != tmp1);

		if (tmp2 != dentry)
			zfsd_abort();
#else
		/* Because FH could not be deleted we can lock it again.  */
		zfsd_mutex_lock(&fh->mutex);
#endif
	}

	if (dentry->deleted)
	{
		/* There already is a thread which tries to delete DENTRY.  */
		zfsd_mutex_unlock(&dentry->fh->mutex);
		RETURN_VOID;
	}

	/* Mark DENTRY as deleted and wake up other threads trying to delete it.  */
	dentry->deleted = true;
	zfsd_cond_broadcast(&dentry->fh->cond);
	dentry_update_cleanup_node(dentry);

	if (dentry->fh->attr.type == FT_DIR)
	{
		/* New subdentries may have been added while we were waiting until the 
		   dentry is unlocked.  */
		if (!internal_dentry_destroy_subdentries(dentry, &tmp_fh, invalidate))
			zfsd_abort();
	}

	if (dentry->parent)
	{
		internal_dentry parent = dentry->parent;

		zfsd_mutex_lock(&parent->fh->mutex);
		internal_dentry_del_from_dir(dentry);
		zfsd_mutex_unlock(&parent->fh->mutex);
	}
	else if (clear_volume_root)
	{
		volume vol;

		vol = volume_lookup(dentry->fh->local_fh.vid);
		if (vol)
		{
			vol->root_dentry = NULL;
			zfsd_mutex_unlock(&vol->mutex);
		}
	}
#ifdef ENABLE_VERSIONS
	if (dentry->dirhtab)
	{
		htab_destroy(dentry->dirhtab);
		dentry->dirhtab = NULL;
	}
#endif

	slot = htab_find_slot_with_hash(dentry_htab, &dentry->fh->local_fh,
									INTERNAL_DENTRY_HASH(dentry), NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif

	dentry->fh->ndentries--;
	if (dentry->next == dentry)
	{
#ifdef ENABLE_CHECKING
		if (dentry->fh->ndentries != 0)
			zfsd_abort();
#endif
		htab_clear_slot(dentry_htab, slot);
		internal_fh_destroy_stage1(dentry->fh);
	}
	else
	{
#ifdef ENABLE_CHECKING
		if (dentry->fh->ndentries == 0)
			zfsd_abort();
#endif
		dentry->next->prev = dentry->prev;
		dentry->prev->next = dentry->next;
		*slot = dentry->next;
	}

	/* Let other threads waiting for DENTRY to finish using DENTRY.  */
	if (invalidate)
	{
		zfsd_mutex_unlock(&fh_mutex);
		fs_invalidate_dentry(dentry, volume_root_p);
	}
	else
	{
		zfsd_mutex_unlock(&dentry->fh->mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}

	/* Because FH could not be destroyed yet we can lock it again.  */
	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&dentry->fh->mutex);

	/* At this moment, we are the only thread which wants do to something with 
	   DENTRY (at least if pthread_mutex is just).  */

	if (dentry->next == dentry)
		internal_fh_destroy_stage2(dentry->fh);
	else
		zfsd_mutex_unlock(&dentry->fh->mutex);

	free(dentry->name.str);
	pool_free(dentry_pool, dentry);
	RETURN_VOID;
}

/*! Create conflict directory for local file handle LOCAL_FH with attributes
   according to ATTR and name NAME in directory DIR on volume VOL. If such
   conflict directory already exists update the local file handle and
   attributes and return it.  */

internal_dentry
create_conflict(volume vol, internal_dentry dir, string * name,
				zfs_fh * local_fh, fattr * attr)
{
	zfs_fh tmp_fh;
	fattr tmp_attr;
	internal_dentry conflict, dentry;
	node nod;

  again:
	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (dir)
	{
		CHECK_MUTEX_LOCKED(&dir->fh->mutex);
	}
#endif

	dentry = dentry_lookup_name(vol, dir, name);
	if (dentry && CONFLICT_DIR_P(dentry->fh->local_fh))
		RETURN_PTR(dentry);

	if (dentry)
	{
		if (!ZFS_FH_EQ(dentry->fh->local_fh, *local_fh))
		{
#ifdef ENABLE_CHECKING
			if (!dir)
				zfsd_abort();
#endif
			tmp_fh = dir->fh->local_fh;
			release_dentry(dir);
			zfsd_mutex_unlock(&vol->mutex);

			internal_dentry_destroy(dentry, true, true,
									dentry->parent == NULL);
			dentry = NULL;
			zfsd_mutex_unlock(&fh_mutex);

#ifdef ENABLE_CHECKING
			if (dir->fh->level == LEVEL_UNLOCKED)
				zfsd_abort();
#endif

			/* This succeeds because DIR was locked so it can't have been
			   deleted meanwhile.  */
			zfs_fh_lookup_nolock(&tmp_fh, &vol, &dir, NULL, false);
		}
		else
			internal_dentry_del_from_dir(dentry);
	}

	tmp_fh.sid = NODE_ID_NONE;
	tmp_fh.vid = vol->id;
	tmp_fh.dev = VIRTUAL_DEVICE;
	tmp_fh.ino = vol->last_conflict_ino;
	tmp_fh.gen = 1;
	tmp_attr.dev = tmp_fh.dev;
	tmp_attr.ino = tmp_fh.ino;
	tmp_attr.version = 0;
	tmp_attr.type = FT_DIR;
	tmp_attr.mode = S_IRWXU | S_IRWXG | S_IRWXO;
	tmp_attr.nlink = 4;
	tmp_attr.uid = attr->uid;
	tmp_attr.gid = attr->gid;
	tmp_attr.rdev = 0;
	tmp_attr.size = 0;
	tmp_attr.blocks = 0;
	tmp_attr.blksize = 4096;
	tmp_attr.atime = time(NULL);
	tmp_attr.ctime = tmp_attr.atime;
	tmp_attr.mtime = tmp_attr.atime;

	conflict = internal_dentry_create(&tmp_fh, &undefined_fh, vol, dir,
									  name, &tmp_attr, NULL, LEVEL_UNLOCKED);

	if (dentry)
	{
		free(dentry->name.str);
		nod = node_lookup(local_fh->sid);
#ifdef ENABLE_CHECKING
		if (!nod)
			zfsd_abort();
#endif
		xstringdup(&dentry->name, &nod->name);
		zfsd_mutex_unlock(&nod->mutex);

		internal_dentry_add_to_dir(conflict, dentry);

		if (dir)
		{
#ifdef ENABLE_CHECKING
			if (dir->fh->level == LEVEL_UNLOCKED
				&& dentry->fh->level == LEVEL_UNLOCKED)
				zfsd_abort();
#endif

			/* Invalidate DENTRY.  */
			tmp_fh = dir->fh->local_fh;
			release_dentry(dir);
		}
#ifdef ENABLE_CHECKING
		else
		{
			if (dentry->fh->level == LEVEL_UNLOCKED)
				zfsd_abort();
		}
#endif

		release_dentry(conflict);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		fs_invalidate_dentry(dentry, dir == NULL);

		if (dir)
		{
			/* This succeeds because DIR or its child was locked so it can't
			   have been deleted meanwhile.  */
			zfs_fh_lookup_nolock(&tmp_fh, &vol, &dir, NULL, false);
		}
		else
		{
			zfsd_mutex_lock(&fh_mutex);
			vol = volume_lookup(tmp_fh.vid);
		}
		goto again;
	}

	RETURN_PTR(conflict);
}

/*! If there is a dentry in place for file FH in conflict directory CONFLICT
   on volume VOL delete it and return NULL. If FH is already there return its
   dentry.  */

static internal_dentry
make_space_in_conflict_dir(volume * volp, internal_dentry * conflictp,
						   bool exists, zfs_fh * fh)
{
	zfs_fh tmp_fh;
	internal_dentry dentry;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*conflictp)->fh->mutex);
#ifdef ENABLE_CHECKING
	if (!CONFLICT_DIR_P((*conflictp)->fh->local_fh))
		zfsd_abort();
	if ((*conflictp)->fh->attr.type != FT_DIR)
		zfsd_abort();
	if (CONFLICT_DIR_P(*fh))
		zfsd_abort();
	if (exists && (*volp)->id != fh->vid)
		zfsd_abort();
#endif

	for (i = 0; i < VARRAY_USED((*conflictp)->fh->subdentries); i++)
	{
		dentry = VARRAY_ACCESS((*conflictp)->fh->subdentries, i,
							   internal_dentry);
		acquire_dentry(dentry);

#ifdef ENABLE_CHECKING
		if (CONFLICT_DIR_P(dentry->fh->local_fh))
			zfsd_abort();
#endif
		if (dentry->fh->local_fh.sid == fh->sid)
		{
			if (!exists || !ZFS_FH_EQ(dentry->fh->local_fh, *fh))
			{
				tmp_fh = (*conflictp)->fh->local_fh;
				release_dentry(*conflictp);
				zfsd_mutex_unlock(&(*volp)->mutex);

				internal_dentry_destroy(dentry, true, true,
										dentry->parent == NULL);

				*volp = volume_lookup(tmp_fh.vid);
				*conflictp = dentry_lookup(&tmp_fh);

				RETURN_PTR(NULL);
			}
			else
				RETURN_PTR(dentry);
		}
		else
			release_dentry(dentry);
	}

	RETURN_PTR(NULL);
}

/*! Add a dentry to conflict dir CONFLICT on volume VOL. If EXISTS is true
   the file really exists so create a dentry with file handle FH, attributes
   ATTR and metadata META; otherwise create a virtual symlink representing
   non-existing file.  */

internal_dentry
add_file_to_conflict_dir(volume vol, internal_dentry conflict, bool exists,
						 zfs_fh * fh, fattr * attr, metadata * meta)
{
	zfs_fh tmp_fh;
	internal_dentry dentry;
	node nod;
	string *name;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);

	dentry = make_space_in_conflict_dir(&vol, &conflict, exists, fh);
	if (dentry)
	{
		if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
			set_attr_version(attr, &dentry->fh->meta);
		dentry->fh->attr = *attr;
		release_dentry(dentry);
		RETURN_PTR(dentry);
	}
#ifdef ENABLE_CHECKING
	else
	{
		if (!vol)
			zfsd_abort();
		if (!conflict)
			zfsd_abort();
	}
#endif

	nod = vol->master;
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);

	if (exists)
	{
		zfs_fh *master_fh;

		if (fh->sid == this_node->id)
		{
			name = &this_node->name;
			master_fh = &undefined_fh;
		}
		else
		{
			name = &nod->name;
			master_fh = fh;
		}

		dentry = internal_dentry_create(fh, master_fh, vol, conflict, name,
										attr, meta, LEVEL_UNLOCKED);
	}
	else
	{
		if (fh->sid == this_node->id)
		{
			name = &this_node->name;
			tmp_fh.sid = this_node->id;
			tmp_fh.ino = nod->id;
		}
		else
		{
			name = &nod->name;
			tmp_fh.sid = nod->id;
			tmp_fh.ino = this_node->id;
		}
		tmp_fh.vid = VOLUME_ID_VIRTUAL;
		tmp_fh.dev = VIRTUAL_DEVICE;
		tmp_fh.gen = 1;
		attr->dev = tmp_fh.dev;
		attr->ino = tmp_fh.ino;
		attr->version = 0;
		attr->type = FT_LNK;
		attr->mode = S_IRWXU | S_IRWXG | S_IRWXO;
		attr->nlink = 1;
		attr->uid = attr->uid;
		attr->gid = attr->gid;
		attr->rdev = 0;
		attr->size = name->len;
		attr->blocks = 0;
		attr->blksize = 4096;
		attr->atime = time(NULL);
		attr->ctime = attr->atime;
		attr->mtime = attr->atime;
		dentry = internal_dentry_create(&tmp_fh, &undefined_fh, vol, conflict,
										name, attr, NULL, LEVEL_UNLOCKED);
	}

	zfsd_mutex_unlock(&nod->mutex);
	release_dentry(dentry);
	RETURN_PTR(dentry);
}

/*! Try resolve CONFLICT on volume VOL, return true if it was resolved.  */

bool try_resolve_conflict(volume vol, internal_dentry conflict)
{
	internal_dentry dentry, dentry2, parent;
	string swp;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);

	switch (VARRAY_USED(conflict->fh->subdentries))
	{
	case 0:
		zfsd_mutex_unlock(&vol->mutex);
		internal_dentry_destroy(conflict, true, true,
								conflict->parent == NULL);
		RETURN_BOOL(true);

	case 1:
		dentry = VARRAY_ACCESS(conflict->fh->subdentries, 0, internal_dentry);
		acquire_dentry(dentry);
		if (REGULAR_FH_P(dentry->fh->local_fh))
		{
			if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
			{
				internal_dentry_del_from_dir(dentry);
				parent = conflict->parent;
				if (parent)
				{
					acquire_dentry(parent);
					internal_dentry_del_from_dir(conflict);

					swp = dentry->name;
					dentry->name = conflict->name;
					conflict->name = swp;

					internal_dentry_add_to_dir(parent, dentry);
					release_dentry(parent);
				}
				else
				{
					swp = dentry->name;
					dentry->name = conflict->name;
					conflict->name = swp;

					vol->root_dentry = dentry;
				}
				release_dentry(dentry);
				zfsd_mutex_unlock(&vol->mutex);
				internal_dentry_destroy(conflict, false, true, parent == NULL);
			}
			else
			{
				release_dentry(dentry);
				zfsd_mutex_unlock(&vol->mutex);
				internal_dentry_destroy(conflict, true, true,
										conflict->parent == NULL);
			}
		}
		else if (NON_EXIST_FH_P(dentry->fh->local_fh))
		{
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			internal_dentry_destroy(conflict, true, true,
									conflict->parent == NULL);
		}
#ifdef ENABLE_CHECKING
		else
			zfsd_abort();
#endif
		RETURN_BOOL(true);

	case 2:
		dentry = VARRAY_ACCESS(conflict->fh->subdentries, 0, internal_dentry);
		dentry2 = VARRAY_ACCESS(conflict->fh->subdentries, 1, internal_dentry);
		acquire_dentry(dentry);
		acquire_dentry(dentry2);

#ifdef ENABLE_CHECKING
		if (!REGULAR_FH_P(dentry->fh->local_fh)
			&& !NON_EXIST_FH_P(dentry->fh->local_fh))
			zfsd_abort();
		if (!REGULAR_FH_P(dentry2->fh->local_fh)
			&& !NON_EXIST_FH_P(dentry2->fh->local_fh))
			zfsd_abort();
#endif

		if (REGULAR_FH_P(dentry->fh->local_fh)
			&& REGULAR_FH_P(dentry2->fh->local_fh))
		{
			/* Force DENTRY to be the local dentry.  */
			if (dentry->fh->local_fh.sid != this_node->id
				&& dentry2->fh->local_fh.sid == this_node->id)
			{
				parent = dentry;
				dentry = dentry2;
				dentry2 = parent;
			}
#ifdef ENABLE_CHECKING
			else if (!(dentry->fh->local_fh.sid == this_node->id
					   && dentry2->fh->local_fh.sid != this_node->id))
				zfsd_abort();
#endif

			if (ZFS_FH_EQ(dentry->fh->meta.master_fh, dentry2->fh->local_fh)
				&& !(dentry->fh->attr.version > dentry->fh->meta.master_version
					 && (dentry2->fh->attr.version
						 > dentry->fh->meta.master_version))
				&& !(METADATA_ATTR_CHANGE_P(dentry->fh->meta,
											dentry->fh->attr)
					 && METADATA_ATTR_CHANGE_P(dentry->fh->meta,
											   dentry2->fh->attr)))
			{
				release_dentry(dentry2);

				internal_dentry_del_from_dir(dentry);
				parent = conflict->parent;
				if (parent)
				{
					acquire_dentry(parent);
					internal_dentry_del_from_dir(conflict);

					swp = dentry->name;
					dentry->name = conflict->name;
					conflict->name = swp;

					internal_dentry_add_to_dir(parent, dentry);
					release_dentry(parent);
				}
				else
				{
					swp = dentry->name;
					dentry->name = conflict->name;
					conflict->name = swp;

					vol->root_dentry = dentry;
				}

				release_dentry(dentry);
				zfsd_mutex_unlock(&vol->mutex);
				internal_dentry_destroy(conflict, false, true, parent == NULL);
				RETURN_BOOL(true);
			}
			else
			{
				release_dentry(dentry);
				release_dentry(dentry2);
				RETURN_BOOL(false);
			}
		}
		if (NON_EXIST_FH_P(dentry->fh->local_fh)
			&& NON_EXIST_FH_P(dentry2->fh->local_fh))
		{
			release_dentry(dentry);
			release_dentry(dentry2);
			zfsd_mutex_unlock(&vol->mutex);
			internal_dentry_destroy(conflict, true, true,
									conflict->parent == NULL);
			RETURN_BOOL(true);
		}
		release_dentry(dentry);
		release_dentry(dentry2);
		break;

	default:
		zfsd_abort();
	}

	RETURN_BOOL(false);
}

/*! Return the local dentry in conflict dir CONFLICT.  */

internal_dentry conflict_local_dentry(internal_dentry conflict)
{
	internal_dentry dentry;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);
#ifdef ENABLE_CHECKING
	if (!CONFLICT_DIR_P(conflict->fh->local_fh))
		zfsd_abort();
#endif

	for (i = 0; i < VARRAY_USED(conflict->fh->subdentries); i++)
	{
		dentry = VARRAY_ACCESS(conflict->fh->subdentries, i, internal_dentry);
		acquire_dentry(dentry);
		if (dentry->fh->local_fh.sid == this_node->id)
			RETURN_PTR(dentry);
		release_dentry(dentry);
	}

	RETURN_PTR(NULL);
}

/*! Return the remote dentry in conflict dir CONFLICT.  */

internal_dentry conflict_remote_dentry(internal_dentry conflict)
{
	internal_dentry dentry;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);
#ifdef ENABLE_CHECKING
	if (!CONFLICT_DIR_P(conflict->fh->local_fh))
		zfsd_abort();
#endif

	for (i = 0; i < VARRAY_USED(conflict->fh->subdentries); i++)
	{
		dentry = VARRAY_ACCESS(conflict->fh->subdentries, i, internal_dentry);
		acquire_dentry(dentry);
		if (dentry->fh->local_fh.sid != this_node->id)
			RETURN_PTR(dentry);
		release_dentry(dentry);
	}

	RETURN_PTR(NULL);
}

/*! Return the other dentry in cronflict dir CONFLICT than DENTRY.  */

internal_dentry
conflict_other_dentry(internal_dentry conflict, internal_dentry dentry)
{
	internal_dentry other;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);
#ifdef ENABLE_CHECKING
	if (!CONFLICT_DIR_P(conflict->fh->local_fh))
		zfsd_abort();
#endif

	for (i = 0; i < VARRAY_USED(conflict->fh->subdentries); i++)
	{
		other = VARRAY_ACCESS(conflict->fh->subdentries, i, internal_dentry);
		if (other != dentry)
		{
			acquire_dentry(other);
			RETURN_PTR(other);
		}
	}

	RETURN_PTR(NULL);
}

/*! Cancel the CONFLICT on volume VOL.  */

void cancel_conflict(volume vol, internal_dentry conflict)
{
	internal_dentry dentry, parent;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&conflict->fh->mutex);

	dentry = conflict_local_dentry(conflict);
	if (dentry)
		internal_dentry_del_from_dir(dentry);

	parent = conflict->parent;
	if (parent)
	{
		acquire_dentry(parent);
		internal_dentry_del_from_dir(conflict);
		if (dentry)
		{
			free(dentry->name.str);
			xstringdup(&dentry->name, &conflict->name);
			internal_dentry_add_to_dir(parent, dentry);
		}
		release_dentry(parent);
	}
	else
	{
		vol->root_dentry = dentry;
	}

	if (dentry)
		release_dentry(dentry);
	zfsd_mutex_unlock(&vol->mutex);

	internal_dentry_destroy(conflict, false, true, parent == NULL);
	zfsd_mutex_unlock(&fh_mutex);

	RETURN_VOID;
}

/*! Hash function for virtual_dir X, computed from FH.  */

static hash_t virtual_dir_hash(const void *x)
{
	const struct virtual_dir_def *vd = (const struct virtual_dir_def *)x;

#ifdef ENABLE_CHECKING
	if (!VIRTUAL_FH_P(vd->fh))
		zfsd_abort();
#endif

	return VIRTUAL_DIR_HASH(vd);
}

/*! Hash function for virtual_dir X, computed from (PARENT->FH, NAME).  */

static hash_t virtual_dir_hash_name(const void *x)
{
	const struct virtual_dir_def *vd = (const struct virtual_dir_def *)x;

#ifdef ENABLE_CHECKING
	if (!vd->parent || !VIRTUAL_FH_P(vd->parent->fh))
		zfsd_abort();
#endif

	return VIRTUAL_DIR_HASH_NAME(vd);
}

/*! Compare a virtual directory XX with client's file handle YY.  */

static int virtual_dir_eq(const void *xx, const void *yy)
{
	const zfs_fh *x = &((const struct virtual_dir_def *)xx)->fh;
	const zfs_fh *y = (const zfs_fh *)yy;

#ifdef ENABLE_CHECKING
	if (!VIRTUAL_FH_P(*x))
		zfsd_abort();
	if (!VIRTUAL_FH_P(*y))
		zfsd_abort();
#endif
	return (x->ino == y->ino && x->dev == y->dev
			&& x->vid == y->vid && x->sid == y->sid
			/* && x->gen == y->gen */ );
}

/*! Compare two virtual directories XX and YY whether they have same parent
   and file name.  */

static int virtual_dir_eq_name(const void *xx, const void *yy)
{
	const struct virtual_dir_def *x = (const struct virtual_dir_def *)xx;
	const struct virtual_dir_def *y = (const struct virtual_dir_def *)yy;

#ifdef ENABLE_CHECKING
	if (!VIRTUAL_FH_P(x->fh))
		zfsd_abort();
	if (!y->parent || !VIRTUAL_FH_P(y->parent->fh))
		zfsd_abort();
#endif

	return (x->parent == y->parent
			&& x->name.len == y->name.len
			&& strcmp(x->name.str, y->name.str) == 0);
}

/*! Create a new virtual directory NAME in virtual directory PARENT.  */

virtual_dir virtual_dir_create(virtual_dir parent, const char *name)
{
	virtual_dir vd;
	static uint32_t last_virtual_ino;
	void **slot;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&parent->mutex);

	last_virtual_ino++;
	if (last_virtual_ino <= ROOT_INODE)
		last_virtual_ino = ROOT_INODE + 1;

	vd = (virtual_dir) pool_alloc(vd_pool);
	vd->fh.sid = NODE_ID_NONE;
	vd->fh.vid = VOLUME_ID_VIRTUAL;
	vd->fh.dev = VIRTUAL_DEVICE;
	vd->fh.ino = last_virtual_ino;
	vd->fh.gen = 1;
	vd->parent = parent;
	xmkstring(&vd->name, name);
	vd->vol = NULL;
	vd->cap = NULL;
	virtual_dir_set_fattr(vd);
	vd->n_mountpoints = 0;
	vd->busy = false;
	vd->users = 0;
	vd->deleted = 0;

	zfsd_mutex_init(&vd->mutex);
	zfsd_mutex_lock(&vd->mutex);

	varray_create(&vd->subdirs, sizeof(virtual_dir), 16);
	vd->subdir_index = VARRAY_USED(parent->subdirs);
	VARRAY_PUSH(parent->subdirs, vd, virtual_dir);
	vd->parent->attr.nlink++;
	vd->parent->attr.ctime = vd->parent->attr.mtime = time(NULL);

	slot = htab_find_slot_with_hash(vd_htab, &vd->fh,
									VIRTUAL_DIR_HASH(vd), INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = vd;

	slot = htab_find_slot(vd_htab_name, vd, INSERT);
#ifdef ENABLE_CHECKING
	if (*slot)
		zfsd_abort();
#endif
	*slot = vd;

	fs_invalidate_fh(&parent->fh);
	RETURN_PTR(vd);
}

/*! Delete a virtual directory VD from all hash tables and free it.  */

void virtual_dir_destroy(virtual_dir vd)
{
	virtual_dir parent;
	void **slot;
	unsigned int count;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vd->mutex);

	/* Check the path to root.  */
	count = 1;
	for (; vd; vd = parent)
	{
		if (vd->busy)
		{
			vd->deleted++;
			zfsd_mutex_unlock(&vd->mutex);
			RETURN_VOID;
		}

		parent = vd->parent;
		if (parent)
			zfsd_mutex_lock(&parent->mutex);
		if (vd->deleted > 1)
			count += vd->deleted - 1;
#ifdef ENABLE_CHECKING
		if (vd->n_mountpoints < count)
			zfsd_abort();
#endif
		vd->n_mountpoints -= count;
		if (vd->n_mountpoints == 0)
		{
			virtual_dir top;

			/* Destroy capability associated with virtual directroy.  */
			if (vd->cap)
			{
				vd->cap->busy = 1;
				put_capability(vd->cap, NULL, vd);
			}

#ifdef ENABLE_CHECKING
			if (VARRAY_USED(vd->subdirs))
				zfsd_abort();
#endif
			varray_destroy(&vd->subdirs);

			/* Remove VD from parent's subdirectories.  */
			top = VARRAY_TOP(vd->parent->subdirs, virtual_dir);
			VARRAY_ACCESS(vd->parent->subdirs, vd->subdir_index, virtual_dir)
				= top;
			VARRAY_POP(vd->parent->subdirs);
			top->subdir_index = vd->subdir_index;
			vd->parent->attr.nlink--;
			vd->parent->attr.ctime = vd->parent->attr.mtime = time(NULL);

			/* Delete the virtual_fh from the table of virtual directories.  */
			slot = htab_find_slot(vd_htab_name, vd, NO_INSERT);
#ifdef ENABLE_CHECKING
			if (!slot)
				zfsd_abort();
#endif
			htab_clear_slot(vd_htab_name, slot);
			slot = htab_find_slot_with_hash(vd_htab, &vd->fh,
											VIRTUAL_DIR_HASH(vd), NO_INSERT);
#ifdef ENABLE_CHECKING
			if (!slot)
				zfsd_abort();
#endif
			htab_clear_slot(vd_htab, slot);

			fs_invalidate_fh(&vd->fh);
			free(vd->name.str);
			zfsd_mutex_unlock(&vd->mutex);
			zfsd_mutex_destroy(&vd->mutex);
			pool_free(vd_pool, vd);
		}
		else
			zfsd_mutex_unlock(&vd->mutex);
	}

	RETURN_VOID;
}

/*! Create the virtual root directory.  */

virtual_dir virtual_root_create(void)
{
	virtual_dir dir;
	void **slot;

	TRACE("");

	zfsd_mutex_lock(&fh_mutex);
	dir = (virtual_dir) pool_alloc(vd_pool);
	dir->fh = root_fh;
	dir->parent = NULL;
	xmkstring(&dir->name, "");
	varray_create(&dir->subdirs, sizeof(virtual_dir), 16);
	dir->subdir_index = 0;
	dir->vol = NULL;
	dir->cap = NULL;
	virtual_dir_set_fattr(dir);
	dir->n_mountpoints = 1;
	dir->busy = false;
	dir->users = 0;
	dir->deleted = 0;

	zfsd_mutex_init(&dir->mutex);

	/* Insert the root into hash table.  */
	slot = htab_find_slot_with_hash(vd_htab, &dir->fh, VIRTUAL_DIR_HASH(dir),
									INSERT);
	*slot = dir;
	zfsd_mutex_unlock(&fh_mutex);

	RETURN_PTR(dir);
}

/*! Destroy virtual root directory.  */

void virtual_root_destroy(virtual_dir dir)
{
	void **slot;

	TRACE("");

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&dir->mutex);

	/* Destroy capability associated with virtual directroy.  */
	if (dir->cap)
	{
		dir->cap->busy = 1;
		put_capability(dir->cap, NULL, dir);
	}

#ifdef ENABLE_CHECKING
	if (VARRAY_USED(dir->subdirs))
		zfsd_abort();
#endif
	varray_destroy(&dir->subdirs);

	slot = htab_find_slot_with_hash(vd_htab, &dir->fh, VIRTUAL_DIR_HASH(dir),
									NO_INSERT);
#ifdef ENABLE_CHECKING
	if (!slot)
		zfsd_abort();
#endif
	htab_clear_slot(vd_htab, slot);
	free(dir->name.str);
	zfsd_mutex_unlock(&dir->mutex);
	zfsd_mutex_destroy(&dir->mutex);
	pool_free(vd_pool, dir);
	zfsd_mutex_unlock(&fh_mutex);

	RETURN_VOID;
}

/*! Create the virtual mountpoint for volume VOL.  */

virtual_dir virtual_mountpoint_create(volume vol)
{
	varray subpath;
	virtual_dir vd, parent, tmp;
	char *s, *mountpoint;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	mountpoint = (char *)xmemdup(vol->mountpoint.str, vol->mountpoint.len + 1);
	varray_create(&subpath, sizeof(string), 8);

	/* Split the path.  */
	s = mountpoint;
	while (*s != 0)
	{
		while (*s == '/')
			s++;

		if (*s == 0)
			break;

		VARRAY_PUSH(subpath, s, char *);
		while (*s != 0 && *s != '/')
			s++;
		if (*s == '/')
			*s++ = 0;
	}

	/* Create the components of the path.  */
	vd = root;
	zfsd_mutex_lock(&root->mutex);
	for (i = 0; i < VARRAY_USED(subpath); i++)
	{
		string str;

		parent = vd;
		s = VARRAY_ACCESS(subpath, i, char *);

		str.str = s;
		str.len = strlen(s);
		vd = vd_lookup_name(parent, &str);
		if (!vd)
			vd = virtual_dir_create(parent, s);
#ifdef ENABLE_CHECKING
		if (!VIRTUAL_FH_P(vd->fh))
			zfsd_abort();
#endif
		zfsd_mutex_unlock(&parent->mutex);
	}
	varray_destroy(&subpath);
	vd->vol = vol;
	vol->root_vd = vd;
	zfsd_mutex_unlock(&vd->mutex);

	/* Increase the count of volumes in subtree.  */
	for (tmp = vd; tmp; tmp = tmp->parent)
	{
		zfsd_mutex_lock(&tmp->mutex);
		tmp->n_mountpoints++;
		zfsd_mutex_unlock(&tmp->mutex);
	}

	free(mountpoint);

	RETURN_PTR(vd);
}

/*! Destroy the virtual mountpoint of volume VOL.  */

void virtual_mountpoint_destroy(volume vol)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);

	if (vol->root_vd)
	{
		zfsd_mutex_lock(&vol->root_vd->mutex);
		virtual_dir_destroy(vol->root_vd);
		vol->root_vd = NULL;
	}
}

/*! Set the file attributes of virtual directory VD.  */

void virtual_dir_set_fattr(virtual_dir vd)
{
	TRACE("");

	vd->attr.dev = vd->fh.dev;
	vd->attr.ino = vd->fh.ino;
	vd->attr.version = 0;
	vd->attr.type = FT_DIR;
	vd->attr.mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	vd->attr.nlink = 2;
	vd->attr.uid = DEFAULT_ZFS_UID;
	vd->attr.gid = DEFAULT_ZFS_GID;
	vd->attr.rdev = 0;
	vd->attr.size = 0;
	vd->attr.blocks = 0;
	vd->attr.blksize = 4096;
	vd->attr.atime = time(NULL);
	vd->attr.mtime = vd->attr.atime;
	vd->attr.ctime = vd->attr.atime;
}

/*! Print the virtual directory VD and its subdirectories to file F indented
   by INDENT spaces.  */

static void
print_virtual_tree_node(FILE * f, virtual_dir vd, unsigned int indent)
{
	unsigned int i;

	for (i = 0; i < indent; i++)
		fputc(' ', f);

	fprintf(f, "'%s'", vd->name.str);
	if (vd->vol)
		fprintf(f, "; VOLUME = '%s'", vd->vol->name.str);
	fputc('\n', f);

	for (i = 0; i < VARRAY_USED(vd->subdirs); i++)
		print_virtual_tree_node(f, VARRAY_ACCESS(vd->subdirs, i, virtual_dir),
								indent + 1);
}

/*! Print the virtual tree to file F.  */

void print_virtual_tree(FILE * f)
{
	print_virtual_tree_node(f, root, 0);
}

/*! Print the virtual tree to STDERR.  */

void debug_virtual_tree(void)
{
	print_virtual_tree(stderr);
}

/*! Initialize data structures in FH.C.  */

void initialize_fh_c(void)
{
	zfs_fh_undefine(undefined_fh);

	/* Data structures for file handles, dentries and virtual directories.  */
	zfsd_mutex_init(&fh_mutex);
	pthread_key_create(&lock_info_key, NULL);
	fh_pool = create_alloc_pool("fh_pool", sizeof(struct internal_fh_def),
								1023, &fh_mutex);
	dentry_pool = create_alloc_pool("dentry_pool",
									sizeof(struct internal_dentry_def),
									1023, &fh_mutex);
	vd_pool = create_alloc_pool("vd_pool", sizeof(struct virtual_dir_def),
								127, &fh_mutex);
	fh_htab = htab_create(250, internal_fh_hash, internal_fh_eq, NULL,
						  &fh_mutex);
	dentry_htab = htab_create(250, internal_dentry_hash, internal_dentry_eq,
							  NULL, &fh_mutex);
	dentry_htab_name = htab_create(250, internal_dentry_hash_name,
								   internal_dentry_eq_name, NULL, &fh_mutex);
	vd_htab = htab_create(100, virtual_dir_hash, virtual_dir_eq, NULL,
						  &fh_mutex);
	vd_htab_name = htab_create(100, virtual_dir_hash_name, virtual_dir_eq_name,
							   NULL, &fh_mutex);

	/* Data structures for cleanup of file handles.  */
	zfsd_mutex_init(&cleanup_dentry_mutex);
	cleanup_dentry_heap = fibheap_new(1020, &cleanup_dentry_mutex);
	// TODO: return value handling and extend function initialize_fh_c for
	// return value
	if (pthread_create
		(&cleanup_dentry_thread, NULL, cleanup_dentry_thread_main, NULL))
	{
		message(LOG_CRIT, FACILITY_THREADING, "pthread_create() failed\n");
	}

	root = virtual_root_create();
}

/*! Destroy data structures in FH.C.  */

void cleanup_fh_c(void)
{
	virtual_root_destroy(root);

	wait_for_thread_to_die(&cleanup_dentry_thread, NULL);

	/* Data structures for file handles, dentries and virtual directories.  */
	zfsd_mutex_lock(&fh_mutex);
#ifdef ENABLE_CHECKING
	if (fh_pool->elts_free < fh_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_MEMORY,
				"Memory leak (%u elements) in fh_pool.\n",
				fh_pool->elts_allocated - fh_pool->elts_free);
	if (dentry_pool->elts_free < dentry_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_MEMORY,
				"Memory leak (%u elements) in dentry_pool.\n",
				dentry_pool->elts_allocated - dentry_pool->elts_free);
	if (vd_pool->elts_free < vd_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_MEMORY,
				"Memory leak (%u elements) in vd_pool.\n",
				vd_pool->elts_allocated - vd_pool->elts_free);
#endif
	htab_destroy(fh_htab);
	htab_destroy(dentry_htab);
	htab_destroy(dentry_htab_name);
	htab_destroy(vd_htab_name);
	htab_destroy(vd_htab);
	free_alloc_pool(fh_pool);
	free_alloc_pool(dentry_pool);
	free_alloc_pool(vd_pool);
	zfsd_mutex_unlock(&fh_mutex);
	zfsd_mutex_destroy(&fh_mutex);
	pthread_key_delete(lock_info_key);

	/* Data structures for cleanup of file handles.  */
	zfsd_mutex_lock(&cleanup_dentry_mutex);
	fibheap_delete(cleanup_dentry_heap);
	zfsd_mutex_unlock(&cleanup_dentry_mutex);
	zfsd_mutex_destroy(&cleanup_dentry_mutex);
}
