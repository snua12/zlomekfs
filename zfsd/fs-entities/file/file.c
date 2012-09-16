/*! \file \brief File operations.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak

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
#include <unistd.h>
#include <inttypes.h>

#include "config.h"
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "pthread-wrapper.h"
#include "constant.h"
#include "memory.h"
#include "alloc-pool.h"
#include "fibheap.h"
#include "hashtab.h"
#include "varray.h"
#include "data-coding.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "configuration.h"
#include "cap.h"
#include "volume.h"
#include "metadata.h"
#include "network.h"
#include "md5.h"
#include "update.h"
#include "reread_config.h"
#include "version.h"
#include "zfs_dirent.h"

/*! The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/*! Heap of opened file descriptors.  */
static fibheap opened;

/*! Mutex protecting access to OPENED.  */
static pthread_mutex_t opened_mutex;

/*! Alloc pool for directory entries.  */
static alloc_pool dir_entry_pool;

/*! Mutex protecting DIR_ENTRY_POOL.  */
static pthread_mutex_t dir_entry_mutex;

/*! Initialize data for file descriptor of file handle FH.  */

static void init_fh_fd_data(internal_fh fh)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (fh->fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&opened_mutex);
	CHECK_MUTEX_LOCKED(&internal_fd_data[fh->fd].mutex);

	internal_fd_data[fh->fd].fd = fh->fd;
	internal_fd_data[fh->fd].generation++;
	fh->generation = internal_fd_data[fh->fd].generation;
	internal_fd_data[fh->fd].heap_node
		= fibheap_insert(opened, (fibheapkey_t) time(NULL),
						 &internal_fd_data[fh->fd]);
}

/*! Close file descriptor FD of local file.  */

static void close_local_fd(int fd)
{
	TRACE("");
#ifdef ENABLE_CHECKING
	if (fd < 0)
		zfsd_abort();
#endif
	CHECK_MUTEX_LOCKED(&opened_mutex);
	CHECK_MUTEX_LOCKED(&internal_fd_data[fd].mutex);

#ifdef ENABLE_CHECKING
	if (internal_fd_data[fd].fd < 0)
		zfsd_abort();
#endif
	internal_fd_data[fd].fd = -1;
	internal_fd_data[fd].generation++;
	close(fd);
	if (internal_fd_data[fd].heap_node)
	{
		fibheap_delete_node(opened, internal_fd_data[fd].heap_node);
		internal_fd_data[fd].heap_node = NULL;
	}
	zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
}

/*! Wrapper for open. If open fails because of too many open file descriptors
   it closes a file descriptor unused for longest time.  */

static int safe_open(const char *pathname, uint32_t flags, uint32_t mode)
{
	int fd;

	TRACE("");

  retry_open:
	fd = open(pathname, flags, mode);
	if ((fd < 0 && errno == EMFILE)
		|| (fd >= 0 && fibheap_size(opened) >= (unsigned int)max_local_fds))
	{
		internal_fd_data_t *fd_data;

		zfsd_mutex_lock(&opened_mutex);
		fd_data = (internal_fd_data_t *) fibheap_extract_min(opened);
#ifdef ENABLE_CHECKING
		if (!fd_data && fibheap_size(opened) > 0)
			zfsd_abort();
#endif
		if (fd_data)
		{
			zfsd_mutex_lock(&fd_data->mutex);
			fd_data->heap_node = NULL;
			if (fd_data->fd >= 0)
				close_local_fd(fd_data->fd);
			else
				zfsd_mutex_unlock(&fd_data->mutex);
		}
		zfsd_mutex_unlock(&opened_mutex);
		if (fd_data)
			goto retry_open;
	}

	RETURN_INT(fd);
}

/*! If local file for file handle FH is opened return true and lock
   INTERNAL_FD_DATA[FH->FD].MUTEX.  */

static bool capability_opened_p(internal_fh fh)
{
	TRACE("");

	if (fh->fd < 0)
		RETURN_BOOL(false);

	zfsd_mutex_lock(&opened_mutex);
	zfsd_mutex_lock(&internal_fd_data[fh->fd].mutex);
	if (fh->generation != internal_fd_data[fh->fd].generation)
	{
		zfsd_mutex_unlock(&internal_fd_data[fh->fd].mutex);
		zfsd_mutex_unlock(&opened_mutex);
		RETURN_BOOL(false);
	}

	internal_fd_data[fh->fd].heap_node
		= fibheap_replace_key(opened, internal_fd_data[fh->fd].heap_node,
							  (fibheapkey_t) time(NULL));
	zfsd_mutex_unlock(&opened_mutex);
	RETURN_BOOL(true);
}

/*! Open local file for dentry DENTRY with additional FLAGS on volume VOL.  */

static int32_t
capability_open(int *fd, uint32_t flags, internal_dentry dentry, volume vol)
{
	string path;
	int err;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (flags & O_CREAT)
		zfsd_abort();
#endif

	if (vol->local_path.str == NULL)
	{
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(ESTALE);
	}

	/* Some flags were specified so close the file descriptor first.  */
	if (flags)
		local_close(dentry->fh);

	else if (capability_opened_p(dentry->fh))
	{
		*fd = dentry->fh->fd;
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(ZFS_OK);
	}

#ifdef ENABLE_VERSIONS
	if (zfs_config.versions.versioning && dentry->version_file
		&& ((flags & O_ACCMODE) != O_RDONLY))
	{
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(EACCES);
	}
	dentry->new_file = false;
#endif

	if (dentry->fh->attr.type == FT_DIR)
		flags |= O_RDONLY;
	else
		/* FIXME: this breaks if the file is unreadable by the owner */
		flags |= O_RDWR;

	build_local_path(&path, vol, dentry);
	dentry->fh->fd = safe_open(path.str, flags, 0);
	err = errno;
	if (dentry->fh->fd >= 0)
	{
		zfsd_mutex_lock(&opened_mutex);
		zfsd_mutex_lock(&internal_fd_data[dentry->fh->fd].mutex);
		init_fh_fd_data(dentry->fh);
		zfsd_mutex_unlock(&opened_mutex);
		*fd = dentry->fh->fd;
#ifdef ENABLE_VERSIONS
		if (zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG))
		{
			// build intervals or mark file size
			if (dentry->version_file)
			{
				if (!dentry->fh->version_path)
					dentry->fh->version_path = xstrdup(path.str);
				version_build_intervals(dentry, vol);
			}
			else
				dentry->fh->marked_size = dentry->fh->attr.size;
		}

		if (zfs_config.versions.versioning && (dentry->fh->attr.type == FT_DIR))
		{
			// store directory path
			if (!dentry->fh->version_path)
				dentry->fh->version_path = xstrdup(path.str);
		}
#endif
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		free(path.str);
		release_dentry(dentry);
		RETURN_INT(ZFS_OK);
	}
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_unlock(&fh_mutex);
	free(path.str);
	release_dentry(dentry);

	if (err == ENOENT || err == ENOTDIR)
		RETURN_INT(ESTALE);

	RETURN_INT(err);
}

/*! Close local file for internal file handle FH.  */

int32_t local_close(internal_fh fh)
{
	TRACE("");
	CHECK_MUTEX_LOCKED(&fh->mutex);

	if (fh->fd >= 0)
	{
#ifdef ENABLE_VERSIONS
		if (zfs_config.versions.versioning && (fh->attr.type == FT_REG) && (fh->version_fd > 0))
			version_close_file(fh, true);
#endif
		zfsd_mutex_lock(&opened_mutex);
		zfsd_mutex_lock(&internal_fd_data[fh->fd].mutex);
		if (fh->generation == internal_fd_data[fh->fd].generation)
			close_local_fd(fh->fd);
		else
			zfsd_mutex_unlock(&internal_fd_data[fh->fd].mutex);
		zfsd_mutex_unlock(&opened_mutex);
		fh->fd = -1;
	}

	RETURN_INT(ZFS_OK);
}

/*! Close remote file for internal capability CAP for dentry DENTRY on volume 
   VOL.  */

static int32_t
remote_close(internal_cap cap, internal_dentry dentry, volume vol)
{
	zfs_cap args;
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;		// WARN: mutex check is after this

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_cap_undefined(cap->master_cap))
		zfsd_abort();
	if (zfs_fh_undefined(cap->master_cap.fh))
		zfsd_abort();
#endif

	args = cap->master_cap;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_unlock(&vol->mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_close_client(t, &args, nod, &fd);

	if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Close remote file for capability CAP and ICAP of dentry DENTRY on volume
   VOL if we are the last user of it.  */

int32_t
cond_remote_close(zfs_cap * cap, internal_cap icap, internal_dentry * dentryp,
				  volume * volp)
{
	int32_t r, r2;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
	if (icap->master_busy == 0)
		zfsd_abort();
	if ((*dentryp)->fh->level == LEVEL_UNLOCKED)
		zfsd_abort();
	if (zfs_fh_undefined((*dentryp)->fh->meta.master_fh))
		zfsd_abort();
	if (zfs_fh_undefined(icap->master_cap.fh)
		|| zfs_cap_undefined(icap->master_cap))
		zfsd_abort();
#endif

	if (icap->master_busy == 1)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_close(icap, *dentryp, *volp);

		r2 = find_capability_nolock(cap, &icap, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif

		if (r != ZFS_OK)
			RETURN_INT(r);

		/* Do not undefine master_cap because it still may be used by a user.
		   We are just closing last "update" use of it. When all uses are
		   closed the capability is destroyed so it is superfluous to undefine 
		   master_cap in that case.  */
	}
#ifdef ENABLE_CHECKING
	else
	{
		if (zfs_fh_undefined(icap->master_cap.fh)
			|| zfs_cap_undefined(icap->master_cap))
			zfsd_abort();
	}
#endif

	icap->master_busy--;
	RETURN_INT(ZFS_OK);
}

/*! Create local file NAME in directory DIR on volume VOL with open flags
   FLAGS, set file attributes according to ATTR.  Store the newly opened file
   descriptor to FDP, create results to RES and metadata to META. If file
   already exists set EXISTS.  */

int32_t
local_create(create_res * res, int *fdp, internal_dentry dir, string * name,
			 uint32_t flags, sattr * attr, volume vol, metadata * meta,
			 bool * exists)
{
	struct stat st;
	string path;
	int32_t r;
	bool existed;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dir->fh->mutex);

	if (vol->local_path.str == NULL)
	{
		release_dentry(dir);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(ESTALE);
	}

	res->dor.file.sid = dir->fh->local_fh.sid;
	res->dor.file.vid = dir->fh->local_fh.vid;

	r = build_local_path_name(&path, vol, dir, name);
	release_dentry(dir);
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_unlock(&fh_mutex);
	if (r < 0)
	{
		RETURN_INT(r);
	}

#ifdef ENABLE_VERSIONS
	if (zfs_config.versions.versioning && strchr(name->str, VERSION_NAME_SPECIFIER_C))
	{
		RETURN_INT(EACCES);
	}
#endif

	existed = (lstat(path.str, &st) == 0);
	if (exists)
		*exists = existed;

	attr->mode = GET_MODE(attr->mode);
	r = safe_open(path.str, O_RDWR | (flags & ~O_ACCMODE), attr->mode);
	if (r < 0)
	{
		free(path.str);
		if (errno == ENOENT || errno == ENOTDIR)
			RETURN_INT(ESTALE);
		RETURN_INT(errno);
	}
	*fdp = r;

	r = local_setattr_path(&res->dor.attr, &path, attr);
	if (r != ZFS_OK)
	{
		close(*fdp);
		if (exists && !*exists)
			unlink(path.str);
		free(path.str);
		RETURN_INT(r);
	}

	free(path.str);
	res->dor.file.dev = res->dor.attr.dev;
	res->dor.file.ino = res->dor.attr.ino;

	vol = volume_lookup(res->dor.file.vid);
#ifdef ENABLE_CHECKING
	if (!vol)
		zfsd_abort();
#endif

	meta->flags = METADATA_COMPLETE;
	meta->modetype = GET_MODETYPE(res->dor.attr.mode, res->dor.attr.type);
	meta->uid = res->dor.attr.uid;
	meta->gid = res->dor.attr.gid;
	if (!lookup_metadata(vol, &res->dor.file, meta, true))
		MARK_VOLUME_DELETE(vol);
	else if (!existed)
	{
		if (!zfs_fh_undefined(meta->master_fh)
			&& !delete_metadata_of_created_file(vol, &res->dor.file, meta))
			MARK_VOLUME_DELETE(vol);
	}
	zfsd_mutex_unlock(&vol->mutex);

	RETURN_INT(ZFS_OK);
}

/*! Create remote file NAME in directory DIR with open flags FLAGS, set file
   attributes according to ATTR.  */

int32_t
remote_create(create_res * res, internal_dentry dir, string * name,
			  uint32_t flags, sattr * attr, volume vol)
{
	create_args args;
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dir->fh->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_fh_undefined(dir->fh->meta.master_fh))
		zfsd_abort();
#endif

	args.where.dir = dir->fh->meta.master_fh;
	args.where.name = *name;
	args.flags = flags;
	args.attr = *attr;

	release_dentry(dir);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_unlock(&node_mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_create_client(t, &args, vol->master, &fd);

	if (r == ZFS_OK)
	{
		if (!decode_create_res(t->dc_reply, res)
			|| !finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}
	if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Create file NAME in directory DIR with open flags FLAGS, set file
   attributes according to ATTR.  */

int32_t
zfs_create(create_res * res, zfs_fh * dir, string * name,
		   uint32_t flags, sattr * attr)
{
	create_res master_res;
	volume vol;
	internal_dentry idir;
	virtual_dir pvd;
	zfs_fh tmp_fh;
	metadata meta;
	int32_t r, r2;
	int fd;
	bool exists;

	TRACE("");

	/* When O_CREAT is NOT set the function zfs_open is called. Force O_CREAT
	   to be set here.  */
	flags |= O_CREAT;

	/* With O_APPEND, data are always written to the end of file and lseek has 
	   no effect on where the data will be written.  */
	flags &= ~O_APPEND;

	r = validate_operation_on_zfs_fh(dir, EROFS, EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	/* Lookup DIR.  */
	r = zfs_fh_lookup_nolock(dir, &vol, &idir, &pvd, true);
	if (r == ZFS_STALE)
	{
#ifdef ENABLE_CHECKING
		if (VIRTUAL_FH_P(*dir))
			zfsd_abort();
#endif
		r = refresh_fh(dir);
		if (r != ZFS_OK)
			RETURN_INT(r);
		r = zfs_fh_lookup_nolock(dir, &vol, &idir, &pvd, true);
	}
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (pvd)
	{
		r = validate_operation_on_virtual_directory(pvd, name, &idir, EROFS);
		if (r != ZFS_OK)
			RETURN_INT(r);
	}
	else
		zfsd_mutex_unlock(&fh_mutex);

	if (idir->fh->attr.type != FT_DIR)
	{
		release_dentry(idir);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(ENOTDIR);
	}

	/* Hide special dirs in the root of the volume.  */
	if (SPECIAL_DIR_P(idir, name->str, true))
	{
		release_dentry(idir);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(EACCES);
	}

	if (idir->fh->meta.flags & METADATA_SHADOW_TREE)
	{
		release_dentry(idir);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(EPERM);
	}

	attr->mode = GET_MODE(attr->mode);
	attr->size = (uint64_t) - 1;
	attr->atime = (zfs_time) - 1;
	attr->mtime = (zfs_time) - 1;

	r = internal_dentry_lock(LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
	if (r != ZFS_OK)
		RETURN_INT(r);

	exists = false;
	if (INTERNAL_FH_HAS_LOCAL_PATH(idir->fh))
	{
		r = update_fh_if_needed(&vol, &idir, &tmp_fh, IFH_ALL_UPDATE);
		if (r != ZFS_OK)
			RETURN_INT(r);
		r = local_create(res, &fd, idir, name, flags, attr, vol, &meta,
						 &exists);
		if (r == ZFS_OK)
			zfs_fh_undefine(master_res.dor.file);
	}
	else if (vol->master != this_node)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_create(res, idir, name, flags, attr, vol);
		if (r == ZFS_OK)
			master_res.dor.file = res->dor.file;
	}
	else
		zfsd_abort();

	r2 = zfs_fh_lookup_nolock(&tmp_fh, &vol, &idir, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	if (r == ZFS_OK)
	{
		internal_cap icap;
		internal_dentry dentry;

		dentry = get_dentry(&res->dor.file, &master_res.dor.file, vol, idir,
							name, &res->dor.attr, &meta);
		icap = get_capability_no_zfs_fh_lookup(&res->cap, dentry,
											   flags & O_ACCMODE);

		if (INTERNAL_FH_HAS_LOCAL_PATH(idir->fh))
		{
#ifdef ENABLE_VERSIONS
			if (!exists)
				dentry->new_file = true;

			if (zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG))
				MARK_FILE_TRUNCATED(dentry->fh);
#endif
			/* Remote file is not open.  */
			zfs_fh_undefine(icap->master_cap.fh);
			zfs_cap_undefine(icap->master_cap);

			if (vol->master != this_node)
			{
				if (!exists)
				{
					if (!add_journal_entry(vol, idir->fh->journal,
										   &idir->fh->local_fh,
										   &dentry->fh->local_fh,
										   &dentry->fh->meta.master_fh,
										   dentry->fh->meta.master_version,
										   name, JOURNAL_OPERATION_ADD))
						MARK_VOLUME_DELETE(vol);
				}
			}
			if (!inc_local_version(vol, idir->fh))
				MARK_VOLUME_DELETE(vol);

			if (vol->master != this_node)
			{
				if (load_interval_trees(vol, dentry->fh))
				{
					local_close(dentry->fh);
					dentry->fh->fd = fd;
					memcpy(res->cap.verify, icap->local_cap.verify,
						   ZFS_VERIFY_LEN);

					zfsd_mutex_lock(&opened_mutex);
					zfsd_mutex_lock(&internal_fd_data[fd].mutex);
					init_fh_fd_data(dentry->fh);
					zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
					zfsd_mutex_unlock(&opened_mutex);
				}
				else
				{
					MARK_VOLUME_DELETE(vol);
					r = ZFS_METADATA_ERROR;
					local_close(dentry->fh);
					close(fd);
				}
			}
			else
			{
				local_close(dentry->fh);
				dentry->fh->fd = fd;
				memcpy(res->cap.verify, icap->local_cap.verify,
					   ZFS_VERIFY_LEN);

				zfsd_mutex_lock(&opened_mutex);
				zfsd_mutex_lock(&internal_fd_data[fd].mutex);
				init_fh_fd_data(dentry->fh);
				zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
				zfsd_mutex_unlock(&opened_mutex);
			}
		}
		else if (vol->master != this_node)
		{
			icap->master_cap = res->cap;
			memcpy(res->cap.verify, icap->local_cap.verify, ZFS_VERIFY_LEN);
		}

		release_dentry(dentry);

		if (INTERNAL_FH_HAS_LOCAL_PATH(idir->fh))
		{
			r2 = update_fh_if_needed(&vol, &idir, &tmp_fh, IFH_REINTEGRATE);
			if (r2 != ZFS_OK)
			{
				r2 = zfs_fh_lookup_nolock(&tmp_fh, &vol, &idir, NULL, false);
#ifdef ENABLE_CHECKING
				if (r2 != ZFS_OK)
					zfsd_abort();
#endif
			}
		}
	}

	internal_dentry_unlock(vol, idir);

	RETURN_INT(r);
}

/*! Open local file for dentry with open flags FLAGS on volume VOL.  */

static int32_t local_open(uint32_t flags, internal_dentry dentry, volume vol)
{
	int32_t r;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	r = capability_open(&fd, flags, dentry, vol);
	if (r == ZFS_OK)
		zfsd_mutex_unlock(&internal_fd_data[fd].mutex);

	RETURN_INT(r);
}

/*! Open remote file for capability ICAP (whose internal dentry is DENTRY)
   with open flags FLAGS on volume VOL.  Store ZFS capability to CAP.  */

static int32_t
remote_open(zfs_cap * cap, internal_cap icap, uint32_t flags,
			internal_dentry dentry, volume vol)
{
	open_args args;
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_fh_undefined(dentry->fh->meta.master_fh))
		zfsd_abort();
#endif

	/* Initialize capability.  */
	icap->master_cap.fh = dentry->fh->meta.master_fh;
	icap->master_cap.flags = icap->local_cap.flags;

	args.file = icap->master_cap.fh;
	args.flags = icap->master_cap.flags | flags;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_unlock(&node_mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_open_client(t, &args, nod, &fd);

	if (r == ZFS_OK)
	{
		if (!decode_zfs_cap(t->dc_reply, cap) || !finish_decoding(t->dc_reply))
		{
			recycle_dc_to_fd(t->dc_reply, fd);
			RETURN_INT(ZFS_INVALID_REPLY);
		}
	}
	if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Open remote file for capability CAP if it is not opened yet. Store its
   dentry to DENTRYP and volume to VOLP.  */

int32_t
cond_remote_open(zfs_cap * cap, internal_cap icap, internal_dentry * dentryp,
				 volume * volp)
{
	zfs_cap master_cap;
	int32_t r, r2;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&(*volp)->mutex);
	CHECK_MUTEX_LOCKED(&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
	if ((*dentryp)->fh->level == LEVEL_UNLOCKED)
		zfsd_abort();
	if (zfs_fh_undefined((*dentryp)->fh->meta.master_fh))
		zfsd_abort();
#endif

	if (icap->master_busy == 0)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_open(&master_cap, icap, 0, *dentryp, *volp);
		if (r != ZFS_OK)
			RETURN_INT(r);

		r2 = find_capability_nolock(cap, &icap, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif

		icap->master_cap = master_cap;
	}
#ifdef ENABLE_CHECKING
	else
	{
		if (zfs_fh_undefined(icap->master_cap.fh)
			|| zfs_cap_undefined(icap->master_cap))
			zfsd_abort();
	}
#endif

	icap->master_busy++;
	RETURN_INT(ZFS_OK);
}

/*! \brief Open file handle FH with open flags FLAGS and return capability in 
   CAP. */
int32_t zfs_open(zfs_cap * cap, zfs_fh * fh, uint32_t flags)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	virtual_dir vd;
	zfs_cap tmp_cap, remote_cap;
	int32_t r, r2;
	bool remote_call = false;

	TRACE("");

	/* When O_CREAT is set the function zfs_create is called. The flag is
	   superfluous here.  */
	flags &= ~O_CREAT;

	/* With O_APPEND, data are always written to the end of file and lseek has 
	   no effect on where the data will be written.  */
	flags &= ~O_APPEND;

	r = validate_operation_on_zfs_fh(fh, ((flags & O_ACCMODE) == O_RDONLY
										  ? ZFS_OK : EISDIR), EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	cap->fh = *fh;
	cap->flags = flags & O_ACCMODE;
	r = get_capability(cap, &icap, &vol, &dentry, &vd, true, true);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (!dentry)
	{
		/* We are opening a pure virtual directory.  */
		if (vol)
			zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&vd->mutex);
		RETURN_INT(ZFS_OK);
	}

	if (CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		/* We are opening a conflict directory.  */
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		if (vd)
			zfsd_mutex_unlock(&vd->mutex);
		RETURN_INT(ZFS_OK);
	}

	if (dentry->fh->attr.type == FT_LNK)
	{
		put_capability(icap, dentry->fh, vd);
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		if (vd)
			zfsd_mutex_unlock(&vd->mutex);
		RETURN_INT(ELOOP);
	}

	r = internal_cap_lock(dentry->fh->attr.type == FT_DIR
						  ? LEVEL_EXCLUSIVE : LEVEL_SHARED,
						  &icap, &vol, &dentry, &vd, &tmp_cap);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (vd)
		zfsd_mutex_unlock(&vd->mutex);

	flags &= ~O_ACCMODE;
	if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
	{
		/* file cached locally and we are not master of this volume */
		if (vol->master != this_node)
		{
			int what;

			/* now decide what to update if needed */
			if ((flags & O_TRUNC)
				&& (cap->flags == O_WRONLY || cap->flags == O_RDWR))
			{
				/* If we are truncating the file synchronize the attributes
				   only and do not synchronize the contents of the file.  */
				what = IFH_METADATA;
			}
			else if (dentry->fh->attr.type == FT_REG)
			{
				/* regular files must get metadata updated to recognize
				   conflict/new version/new file size update gets scheduled
				   for read-ahead from server reintegration when opening seems 
				   not smart, so no scheduling for that */
				what = IFH_METADATA | IFH_UPDATE;
			}
			else
			{
				/* the rest should get fully updated (directories for
				   example...) */
				what = IFH_ALL_UPDATE;
			}

			/* determine what needs updating and do it if it's what we just
			   decided */
			r = update_cap_if_needed(&icap, &vol, &dentry, &vd, &tmp_cap, true,
									 what);
			if (r != ZFS_OK)
				RETURN_INT(r);

			switch (dentry->fh->attr.type)
			{
			case FT_REG:
				if (load_interval_trees(vol, dentry->fh))
				{
					r = local_open(flags, dentry, vol);
				}
				else
				{
					MARK_VOLUME_DELETE(vol);
					r = ZFS_METADATA_ERROR;
				}
				break;

			case FT_DIR:
				r = local_open(flags, dentry, vol);
				break;

			case FT_BLK:
			case FT_CHR:
			case FT_SOCK:
			case FT_FIFO:
				if (volume_master_connected(vol))
				{
					zfsd_mutex_unlock(&fh_mutex);
					r = remote_open(&remote_cap, icap, flags, dentry, vol);
					remote_call = true;
				}
				else
					r = local_open(flags, dentry, vol);
				break;

			default:
				zfsd_abort();
			}
		}
		else
			/* we are master of the volume, nothing to update */
			r = local_open(flags, dentry, vol);
	}
	else if (vol->master != this_node)
	{
		/* file not cached locally and we are not master */
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_open(&remote_cap, icap, flags, dentry, vol);
		remote_call = true;
	}
	else
		/* file not cached locally but we are master volume? can't happen! */
		zfsd_abort();

	r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	if (r == ZFS_OK)
	{
		if (remote_call)
			icap->master_cap = remote_cap;
		else if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
		{
			if ((flags & O_TRUNC)
				&& (cap->flags == O_WRONLY || cap->flags == O_RDWR))
			{
				/* If the file was truncated, increase its version and delete
				   the contents of interval trees.  */

				if (!inc_local_version(vol, dentry->fh))
					MARK_VOLUME_DELETE(vol);

				if (dentry->fh->updated)
				{
					interval_tree_delete(dentry->fh->updated, 0, UINT64_MAX);
					if (dentry->fh->updated->deleted)
					{
						if (!flush_interval_tree(vol, dentry->fh,
												 METADATA_TYPE_UPDATED))
							MARK_VOLUME_DELETE(vol);
					}
				}
				if (dentry->fh->modified)
				{
					interval_tree_delete(dentry->fh->modified, 0, UINT64_MAX);
					if (dentry->fh->modified->deleted)
					{
						if (!flush_interval_tree(vol, dentry->fh,
												 METADATA_TYPE_MODIFIED))
							MARK_VOLUME_DELETE(vol);
					}
				}
			}
		}
	}
	else
	{
		if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh) && vol->master != this_node)
		{
			if (dentry->fh->attr.type == FT_REG
				&& !save_interval_trees(vol, dentry->fh))
			{
				MARK_VOLUME_DELETE(vol);
				r = ZFS_METADATA_ERROR;
			}
		}

		put_capability(icap, dentry->fh, vd);
	}

	internal_cap_unlock(vol, dentry, vd);

	RETURN_INT(r);
}

/*! Close capability CAP.  */

int32_t zfs_close(zfs_cap * cap)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	virtual_dir vd;
	zfs_cap tmp_cap;
	int32_t r, r2;

	TRACE("");

	r = validate_operation_on_zfs_fh(&cap->fh, ZFS_OK, EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	r = find_capability(cap, &icap, &vol, &dentry, &vd, true);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (!dentry)
	{
		/* We are closing a pure virtual directory.  */
		put_capability(icap, NULL, vd);
		if (vol)
			zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&vd->mutex);
		RETURN_INT(ZFS_OK);
	}

	if (CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		/* We are closing a conflict directory.  */
		put_capability(icap, dentry->fh, vd);
		if (vd)
			zfsd_mutex_unlock(&vd->mutex);
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(ZFS_OK);
	}

	r = internal_cap_lock(LEVEL_SHARED, &icap, &vol, &dentry, &vd, &tmp_cap);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (vd)
		zfsd_mutex_unlock(&vd->mutex);

	if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
	{
		if (!zfs_cap_undefined(icap->master_cap)
			&& (dentry->fh->attr.type == FT_BLK
				|| dentry->fh->attr.type == FT_CHR
				|| dentry->fh->attr.type == FT_SOCK
				|| dentry->fh->attr.type == FT_FIFO))
		{
			zfsd_mutex_unlock(&fh_mutex);
			r = remote_close(icap, dentry, vol);

			r2 = find_capability(&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
				zfsd_abort();
#endif
		}
		else if (icap->master_close_p)
		{
			r = cond_remote_close(&tmp_cap, icap, &dentry, &vol);
			if (r == ZFS_OK)
				icap->master_close_p = false;
			zfsd_mutex_unlock(&fh_mutex);
		}
		else
		{
			zfsd_mutex_unlock(&fh_mutex);
			r = ZFS_OK;
		}

		if (icap->busy == 1)
		{
			if (vol->master != this_node)
			{
				if (dentry->fh->attr.type == FT_REG
					&& !save_interval_trees(vol, dentry->fh))
					MARK_VOLUME_DELETE(vol);
			}
#ifdef ENABLE_VERSIONS
			if (zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG)
				&& (dentry->fh->version_fd > 0))
				version_save_interval_trees(dentry->fh);
			// we are generating new version files
			if (zfs_config.versions.versioning)
			{
				dentry->version_dirty = true;
				dentry->new_file = false;
				UNMARK_FILE_TRUNCATED(dentry->fh);
			}
#endif
			zfsd_mutex_unlock(&vol->mutex);
			r = local_close(dentry->fh);
		}
		else
		{
			zfsd_mutex_unlock(&vol->mutex);
		}
		release_dentry(dentry);
	}
	else if (vol->master != this_node)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_close(icap, dentry, vol);
	}
	else
		zfsd_abort();

	r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	/* Reread config file.  */
	if (cap->fh.vid == VOLUME_ID_CONFIG
		&& (cap->flags == O_WRONLY || cap->flags == O_RDWR))
		add_reread_config_request_dentry(dentry);

	if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
		&& dentry->fh->attr.type == FT_REG
		&& (dentry->fh->meta.flags & METADATA_MODIFIED_TREE)
		&& (cap->flags == O_WRONLY || cap->flags == O_RDWR))
	{
		r2 = update_cap_if_needed(&icap, &vol, &dentry, &vd, &tmp_cap,
								  r == ZFS_OK, IFH_REINTEGRATE);
		if (r2 != ZFS_OK)
			RETURN_INT(r);
	}

	if (r == ZFS_OK)
		put_capability(icap, dentry->fh, vd);

	internal_cap_unlock(vol, dentry, vd);

	RETURN_INT(r);
}

/*! Encode one directory entry (INO, COOKIE, NAME[NAME_LEN]) to DC
   LIST->BUFFER. Additional data is passed in DATA.  */

bool
filldir_encode(uint32_t ino, int32_t cookie, const char *name,
			   uint32_t name_len, dir_list * list, readdir_data * data)
{
	DC *dc = (DC *) list->buffer;
	char *old_pos;
	unsigned int old_len;
	dir_entry entry;

#ifdef ENABLE_CHECKING
	if (name[0] == 0)
		zfsd_abort();
#endif

	entry.ino = ino;
	entry.cookie = cookie;
	entry.name.str = CAST_QUAL(char *, name);
	entry.name.len = name_len;

	/* Try to encode ENTRY to DC.  */
	old_pos = dc->cur_pos;
	old_len = dc->cur_length;
	if (!encode_dir_entry(dc, &entry)
		|| data->written + dc->cur_length - old_len > data->count)
	{
		/* There is not enough space in DC to encode ENTRY.  */
		dc->cur_pos = old_pos;
		dc->cur_length = old_len;
		return false;
	}
	else
	{
		list->n++;
		data->written += dc->cur_length - old_len;
	}
	return true;
}

/*! Store one directory entry (INO, COOKIE, NAME[NAME_LEN]) to array
   LIST->BUFFER.  */

bool
filldir_array(uint32_t ino, int32_t cookie, const char *name,
			  uint32_t name_len, dir_list * list,
			  ATTRIBUTE_UNUSED readdir_data * data)
{
	dir_entry *entries = (dir_entry *) list->buffer;

	if (list->n >= ZFS_MAX_DIR_ENTRIES)
		return false;

	entries[list->n].ino = ino;
	entries[list->n].cookie = cookie;
	entries[list->n].name.str = (char *)xmemdup(name, name_len + 1);
	entries[list->n].name.len = name_len;
	list->n++;
	return true;
}

/*! Hash function for directory entry ENTRY.  */
#define FILLDIR_HTAB_HASH(ENTRY)                                        \
  crc32_buffer ((ENTRY)->name.str, (ENTRY)->name.len)

/*! Hash function for directory entry X being inserted for filldir htab.  */

hash_t filldir_htab_hash(const void *x)
{
	return FILLDIR_HTAB_HASH((const dir_entry *)x);
}

/*! Compare directory entries XX and YY.  */

int filldir_htab_eq(const void *xx, const void *yy)
{
	const dir_entry *x = (const dir_entry *)xx;
	const dir_entry *y = (const dir_entry *)yy;

	return (x->name.len == y->name.len
			&& memcmp(x->name.str, y->name.str, x->name.len) == 0);
}

/*! Free directory entry XX.  */

void filldir_htab_del(void *xx)
{
	dir_entry *x = (dir_entry *) xx;

	free(x->name.str);
	zfsd_mutex_lock(&dir_entry_mutex);
	pool_free(dir_entry_pool, x);
	zfsd_mutex_unlock(&dir_entry_mutex);
}

/*! Store one directory entry (INO, COOKIE, NAME[NAME_LEN]) to hash table
   LIST->BUFFER.  */

bool
filldir_htab(uint32_t ino, int32_t cookie, const char *name,
			 uint32_t name_len, dir_list * list,
			 ATTRIBUTE_UNUSED readdir_data * data)
{
	filldir_htab_entries *entries = (filldir_htab_entries *) list->buffer;
	dir_entry *entry;
	void **slot;

	entries->last_cookie = cookie;

	/* Do not add "." and "..".  */
	if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
		return true;

	zfsd_mutex_lock(&dir_entry_mutex);
	entry = (dir_entry *) pool_alloc(dir_entry_pool);
	zfsd_mutex_unlock(&dir_entry_mutex);
	entry->ino = ino;
	entry->cookie = cookie;
	entry->name.str = (char *)xmemdup(name, name_len + 1);
	entry->name.len = name_len;

	slot = htab_find_slot_with_hash(entries->htab, entry,
									FILLDIR_HTAB_HASH(entry), INSERT);
	if (*slot)
	{
		htab_clear_slot(entries->htab, slot);
		list->n--;
	}

	*slot = entry;
	list->n++;

	return true;
}

/*! Read DATA->COUNT bytes from virtual directory VD starting at position
   COOKIE.  Store directory entries to LIST using function FILLDIR.  */

static bool
read_virtual_dir(dir_list * list, virtual_dir vd, int32_t cookie,
				 readdir_data * data, filldir_f filldir)
{
	uint32_t ino;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vd->mutex);

	if (cookie > 0)
		RETURN_BOOL(true);

	switch (cookie)
	{
	case 0:
		cookie--;
		if (!(*filldir) (vd->fh.ino, cookie, ".", 1, list, data))
			RETURN_BOOL(false);
		/* Fallthru.  */

	case -1:
		if (vd->parent)
		{
			zfsd_mutex_lock(&vd->parent->mutex);
			ino = vd->parent->fh.ino;
			zfsd_mutex_unlock(&vd->parent->mutex);
		}
		else
			ino = vd->fh.ino;

		cookie--;
		if (!(*filldir) (ino, cookie, "..", 2, list, data))
			RETURN_BOOL(false);
		/* Fallthru.  */

	default:
		for (i = -cookie - 2; i < VARRAY_USED(vd->subdirs); i++)
		{
			virtual_dir svd;

			svd = VARRAY_ACCESS(vd->subdirs, i, virtual_dir);
			zfsd_mutex_lock(&svd->mutex);
			cookie--;
			if (!(*filldir) (svd->fh.ino, cookie, svd->name.str,
							 svd->name.len, list, data))
			{
				zfsd_mutex_unlock(&svd->mutex);
				RETURN_BOOL(false);
			}
			zfsd_mutex_unlock(&svd->mutex);
		}
		if (i >= VARRAY_USED(vd->subdirs))
			list->eof = 1;
		break;
	}

	RETURN_BOOL(true);
}

/*! Read DATA->COUNT bytes from conflict directory IDIR on volume VOL
   starting at position COOKIE.  Store directory entries to LIST using
   function FILLDIR.  */

static bool
read_conflict_dir(dir_list * list, internal_dentry idir, virtual_dir vd,
				  int32_t cookie, readdir_data * data, volume vol,
				  filldir_f filldir)
{
	uint32_t ino;
	unsigned int i;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&idir->fh->mutex);

	if (vd)
	{
		if (!read_virtual_dir(list, vd, cookie, data, filldir))
			RETURN_BOOL(false);
		if (cookie < 2)
			cookie = 2;
	}

	list->eof = 0;
	if (cookie < 0)
		cookie = 0;

	switch (cookie)
	{
	case 0:
		cookie++;
		if (!(*filldir) (idir->fh->local_fh.ino, cookie, ".", 1, list, data))
			RETURN_BOOL(false);
		/* Fallthru.  */

	case 1:
		if (idir->parent)
		{
			zfsd_mutex_lock(&idir->parent->fh->mutex);
			ino = idir->parent->fh->local_fh.ino;
			zfsd_mutex_unlock(&idir->parent->fh->mutex);
		}
		else
		{
			virtual_dir pvd;

			/* This is safe because the virtual directory can't be destroyed
			   while volume is locked.  */
			pvd = vol->root_vd->parent ? vol->root_vd->parent : vol->root_vd;
			ino = pvd->fh.ino;
		}

		cookie++;
		if (!(*filldir) (ino, cookie, "..", 2, list, data))
			RETURN_BOOL(false);
		/* Fallthru.  */

	default:
		for (i = cookie - 2; i < VARRAY_USED(idir->fh->subdentries); i++)
		{
			internal_dentry dentry;

			dentry = VARRAY_ACCESS(idir->fh->subdentries, i, internal_dentry);
			zfsd_mutex_lock(&dentry->fh->mutex);

			if (vd)
			{
				virtual_dir svd;
				svd = vd_lookup_name(vd, &dentry->name);
				if (svd)
				{
					zfsd_mutex_unlock(&svd->mutex);
					zfsd_mutex_unlock(&dentry->fh->mutex);
					continue;
				}
			}

			cookie++;
			if (!(*filldir) (dentry->fh->local_fh.ino, cookie,
							 dentry->name.str, dentry->name.len, list, data))
			{
				zfsd_mutex_unlock(&dentry->fh->mutex);
				RETURN_BOOL(false);
			}
			zfsd_mutex_unlock(&dentry->fh->mutex);
		}
		if (i >= VARRAY_USED(idir->fh->subdentries))
			list->eof = 1;
		break;
	}

	RETURN_BOOL(true);
}

/*! Read COUNT bytes from local directory with DENTRY and virtual directory
   VD on volume VOL starting at position COOKIE. Store directory entries to
   LIST using function FILLDIR.  */

int32_t
local_readdir(dir_list * list, internal_dentry dentry, virtual_dir vd,
			  zfs_fh * fh, int32_t cookie, readdir_data * data, volume vol,
			  filldir_f filldir,
			  ATTRIBUTE_UNUSED_VERSIONS bool convert_versions)
{
	int32_t r = ZFS_OK;
	int fd;
	bool local_volume_root;
	bool is_vername = false;
	char *vername = NULL;
#ifdef ENABLE_VERSIONS
	char *vs;
	bool store = false;
	time_t stamp;
	bool local_verdisplay = zfs_config.versions.verdisplay;
#endif

	TRACE("");
#ifdef ENABLE_CHECKING
	CHECK_MUTEX_LOCKED(&fh_mutex);
	if (vol)
	{
		CHECK_MUTEX_LOCKED(&vol->mutex);
	}
	if (dentry)
	{
		CHECK_MUTEX_LOCKED(&dentry->fh->mutex);
	}
	if (vd)
	{
		CHECK_MUTEX_LOCKED(&vd->mutex);
	}
#endif

	if (vd)
	{
		if (!read_virtual_dir(list, vd, cookie, data, filldir))
		{
			zfsd_mutex_unlock(&vd->mutex);
			if (dentry)
				release_dentry(dentry);
			zfsd_mutex_unlock(&fh_mutex);
			if (vol)
				zfsd_mutex_unlock(&vol->mutex);
			RETURN_INT(list->n == 0 ? EINVAL : ZFS_OK);
		}

		zfsd_mutex_unlock(&vd->mutex);
		if (!dentry)
		{
			zfsd_mutex_unlock(&fh_mutex);
			if (vol)
				zfsd_mutex_unlock(&vol->mutex);
		}
	}

	if (dentry)
	{
#ifdef ENABLE_VERSIONS
		if (convert_versions && zfs_config.versions.versioning && dentry->dirstamp
			&& (dentry->dirstamp != VERSION_LIST_VERSIONS_STAMP))
		{
			if (!cookie)
			{
				store = true;
				version_create_dirhtab(dentry);
			}
			else
			{
				r = version_readdir_from_dirhtab(list, dentry, cookie, data,
												 filldir);

				release_dentry(dentry);
				zfsd_mutex_unlock(&fh_mutex);
				if (vol)
					zfsd_mutex_unlock(&vol->mutex);

				RETURN_INT(r);
			}
		}
#endif

		local_volume_root = LOCAL_VOLUME_ROOT_P(dentry);

		r = capability_open(&fd, 0, dentry, vol);
		if (r != ZFS_OK)
			RETURN_INT(r);

		list->eof = 0;
		if (cookie < 0)
			cookie = 0;

#ifdef ENABLE_VERSIONS
		// if new version files were created since previous readdir, we will
		// start again
		if (convert_versions && zfs_config.versions.versioning && dentry->version_dirty && cookie)
			cookie = 0;

		dentry->version_dirty = false;

		if (convert_versions && zfs_config.versions.versioning && !zfs_config.versions.verdisplay)
		{
			// should we display versions no matter what was specified to
			// zfsd?
			char *x;
			struct stat st;

			if (dentry->dirstamp == VERSION_LIST_VERSIONS_STAMP)
				local_verdisplay = true;
			else
			{
				acquire_dentry(dentry);
				x = xstrconcat(dentry->fh->version_path,
							   DIRECTORY_SEPARATOR, VERSION_DISPLAY_FILE, NULL);
				release_dentry(dentry);
				if (!lstat(x, &st))
					local_verdisplay = true;
				free(x);
			}
		}
#endif

		int dup_fd = dup(fd);
		if (dup_fd == -1)
		{
			zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
			RETURN_INT (errno);
		}	

		DIR * dirp = zfs_fdopendir(dup_fd);
		if (dirp == NULL)
		{    
			zfs_closedir(dirp);
			zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
			RETURN_INT (errno);
		}    

		zfs_seekdir(dirp, cookie);

		while (1)
		{
			struct dirent entry, *de; 
			r = zfs_readdir_r(dirp, &entry, &de);
			if (r > 0) // readdir_r has failed
			{
				break;
			}
			else if (r == 0 && de == NULL) // end of list
			{
				list->eof = 1;
				break;
			}

			cookie = zfs_telldir(dirp);

			/* not used long block_start; */

			/* Hide special dirs in the root of the volume.  */
			if (local_volume_root && SPECIAL_NAME_P(de->d_name, false))
				continue;
#ifdef ENABLE_VERSIONS
			stamp = 0;
			if (convert_versions && zfs_config.versions.versioning)
			{
				/* Omit versions that did not exist in the specified time. 
				 */
				if (store && dentry->dirstamp
					&& (dentry->dirstamp != VERSION_LIST_VERSIONS_STAMP))
				{
					char *f;
					struct stat st;

					f = xstrconcat(dentry->fh->version_path,
								   DIRECTORY_SEPARATOR, de->d_name, NULL);
					if (!lstat(f, &st) && (st.st_mtime > dentry->dirstamp))
					{
						free(f);
						continue;
					}
					free(f);
				}

				/* Hide version files or convert their names or select
				   them for storage.  */
				if ((vs = strchr(de->d_name, VERSION_NAME_SPECIFIER_C)))
				{
					// convert stamp to string
					struct tm tm;
					char ts[VERSION_MAX_SPECIFIER_LENGTH];
					char *q;

					// skip interval files
					q = strchr(vs + 1, '.');
					if (q)
						continue;

					stamp = atoi(vs + 1);

					if (zfs_config.versions.retention_age_max > 0)
					{
						if ((time(NULL) - stamp) > zfs_config.versions.retention_age_max)
							if (version_retent_file
								(dentry, vol, de->d_name))
								continue;
					}

					if (store)
					{
						/* Return only newer versions.  */
						if (stamp < dentry->dirstamp)
							continue;

						*vs = '\0';
					}
					else if (local_verdisplay)
					{
						localtime_r(&stamp, &tm);
						strftime(ts, sizeof(ts), VERSION_TIMESTAMP, &tm);

						*(vs + 1) = '\0';
						vername = xstrconcat(de->d_name, ts, NULL);
						is_vername = true;
					}
					else
						continue;
				}
				/* Skip current versions if @versions is specified.  */
				else if (dentry->dirstamp == VERSION_LIST_VERSIONS_STAMP)
					continue;
			}
#endif

			if (vd)
			{
				virtual_dir svd;
				string name;

				/* Hide "." and "..".  */
				if (de->d_name[0] == '.'
					&& (de->d_name[1] == 0
						|| (de->d_name[1] == '.' && de->d_name[2] == 0)))
					continue;

				if (zfs_fh_lookup_nolock(fh, NULL, NULL, &vd, false)
					== ZFS_OK)
				{
					/* Hide files which have the same name like some
					   virtual directory.  */
					name.str = de->d_name;
					name.len = strlen(de->d_name);
					svd = vd_lookup_name(vd, &name);
					zfsd_mutex_unlock(&vd->mutex);
					zfsd_mutex_unlock(&fh_mutex);
					if (svd)
					{
						zfsd_mutex_unlock(&svd->mutex);
						continue;
					}
				}
			}

			if (!is_vername)
				vername = de->d_name;

#ifdef ENABLE_VERSIONS
			// store in a hash table, if not '.' and '..'
			if (convert_versions && store &&
				!(de->d_name[0] == '.' &&
				  (de->d_name[1] == 0
				   || (de->d_name[1] == '.' && de->d_name[2] == 0))))
			{
				version_readdir_fill_dirhtab(dentry, stamp, de->d_ino, de->d_name);
			}
			else
#endif
			if (!(*filldir) (de->d_ino, cookie, vername, strlen(vername), list, data))
			{
				if (is_vername)
					free(vername);
				break;
			}

			if (is_vername)
				free(vername);
		}

		zfs_closedir(dirp);
		zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
	}

	RETURN_INT(r);
}

/*! Read COUNT bytes from remote directory CAP of dentry DENTRY on volume VOL
   starting at position COOKIE. Store directory entries to LIST using function
   FILLDIR.  */

int32_t
remote_readdir(dir_list * list, internal_cap cap, internal_dentry dentry,
			   int32_t cookie, readdir_data * data, volume vol,
			   filldir_f filldir)
{
	read_dir_args args;
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_cap_undefined(cap->master_cap))
		zfsd_abort();
	if (zfs_fh_undefined(cap->master_cap.fh))
		zfsd_abort();
#endif

	args.cap = cap->master_cap;
	args.cookie = cookie;
	args.count = data->count;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_unlock(&vol->mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_readdir_client(t, &args, nod, &fd);

	if (r == ZFS_OK)
	{
		if (filldir == &filldir_encode)
		{
			DC *dc = (DC *) list->buffer;

			if (!decode_dir_list(t->dc_reply, list))
				r = ZFS_INVALID_REPLY;
			else if (t->dc_reply->max_length > t->dc_reply->cur_length)
			{
				memcpy(dc->cur_pos, t->dc_reply->cur_pos,
					   t->dc_reply->max_length - t->dc_reply->cur_length);
				dc->cur_pos +=
					t->dc_reply->max_length - t->dc_reply->cur_length;
				dc->cur_length +=
					t->dc_reply->max_length - t->dc_reply->cur_length;
			}
		}
		else if (filldir == &filldir_array)
		{
			if (!decode_dir_list(t->dc_reply, list))
				r = ZFS_INVALID_REPLY;
			else
			{
				uint32_t i;
				dir_entry *entries = (dir_entry *) list->buffer;

				if (list->n <= ZFS_MAX_DIR_ENTRIES)
				{
					for (i = 0; i < list->n; i++)
					{
						if (!decode_dir_entry(t->dc_reply, &entries[i]))
						{
							list->n = i;
							r = ZFS_INVALID_REPLY;
							break;
						}
						else
							xstringdup(&entries[i].name, &entries[i].name);
					}
					if (!finish_decoding(t->dc_reply))
						r = ZFS_INVALID_REPLY;
				}
				else
					r = ZFS_INVALID_REPLY;
			}
		}
		else if (filldir == &filldir_htab)
		{
			dir_list tmp;

			if (!decode_dir_list(t->dc_reply, &tmp))
				r = ZFS_INVALID_REPLY;
			else
			{
				uint32_t i;

				list->eof = tmp.eof;
				for (i = 0; i < tmp.n; i++)
				{
					filldir_htab_entries *entries
						= (filldir_htab_entries *) list->buffer;
					dir_entry *entry;
					void **slot;

					zfsd_mutex_lock(&dir_entry_mutex);
					entry = (dir_entry *) pool_alloc(dir_entry_pool);
					zfsd_mutex_unlock(&dir_entry_mutex);

					if (!decode_dir_entry(t->dc_reply, entry))
					{
						r = ZFS_INVALID_REPLY;
						zfsd_mutex_lock(&dir_entry_mutex);
						pool_free(dir_entry_pool, entry);
						zfsd_mutex_unlock(&dir_entry_mutex);
						break;
					}

					entries->last_cookie = entry->cookie;

					/* Do not add "." and "..".  */
					if (entry->name.str[0] == '.'
						&& (entry->name.str[1] == 0
							|| (entry->name.str[1] == '.'
								&& entry->name.str[2] == 0)))
					{
						zfsd_mutex_lock(&dir_entry_mutex);
						pool_free(dir_entry_pool, entry);
						zfsd_mutex_unlock(&dir_entry_mutex);
						continue;
					}

					xstringdup(&entry->name, &entry->name);
					slot = htab_find_slot_with_hash(entries->htab, entry,
													FILLDIR_HTAB_HASH(entry),
													INSERT);
					if (*slot)
					{
						htab_clear_slot(entries->htab, slot);
						list->n--;
					}

					*slot = entry;
					list->n++;
				}
				if (!finish_decoding(t->dc_reply))
					r = ZFS_INVALID_REPLY;
			}
		}
		else
			zfsd_abort();
	}
	else if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Read COUNT bytes from directory CAP starting at position COOKIE. Store
   directory entries to LIST using function FILLDIR.  */

int32_t
zfs_readdir(dir_list * list, zfs_cap * cap, int32_t cookie, uint32_t count,
			filldir_f filldir)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	virtual_dir vd;
	readdir_data data;
	zfs_cap tmp_cap;
	int32_t r, r2;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (list->n != 0 || list->eof != 0 || list->buffer == 0)
		zfsd_abort();
#endif

	if (cap->flags != O_RDONLY)
		RETURN_INT(EBADF);

	r = validate_operation_on_zfs_fh(&cap->fh, ZFS_OK, EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	r = find_capability_nolock(cap, &icap, &vol, &dentry, &vd, true);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (dentry)
	{
		zfsd_mutex_unlock(&fh_mutex);
		if (dentry->fh->attr.type != FT_DIR)
		{
			if (vd)
				zfsd_mutex_unlock(&vd->mutex);
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			RETURN_INT(ENOTDIR);
		}

		r = internal_cap_lock(LEVEL_SHARED, &icap, &vol, &dentry, &vd,
							  &tmp_cap);
		if (r != ZFS_OK)
			RETURN_INT(r);
	}

	data.written = 0;
	data.count = (count > ZFS_MAXDATA) ? ZFS_MAXDATA : count;

	if (dentry && CONFLICT_DIR_P(dentry->fh->local_fh))
	{
		if (!read_conflict_dir(list, dentry, vd, cookie, &data, vol, filldir))
			r = (list->n == 0) ? EINVAL : ZFS_OK;
		else
			r = ZFS_OK;
		if (vd)
			zfsd_mutex_unlock(&vd->mutex);
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
	}
	else if (!dentry || INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
	{
		r = local_readdir(list, dentry, vd, &tmp_cap.fh, cookie, &data, vol,
						  filldir, true);
	}
	else if (vol->master != this_node)
	{
		if (vd)
			zfsd_mutex_unlock(&vd->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_readdir(list, icap, dentry, cookie, &data, vol, filldir);
	}
	else
		zfsd_abort();

	/* Cleanup decoded directory entries on error.  */
	if (r != ZFS_OK && list->n > 0)
	{
		if (filldir == &filldir_array)
		{
			uint32_t i;
			dir_entry *entries = (dir_entry *) list->buffer;

			for (i = 0; i < list->n; i++)
				free(entries[i].name.str);
		}
		else if (filldir == &filldir_htab)
		{
			filldir_htab_entries *entries
				= (filldir_htab_entries *) list->buffer;

			htab_empty(entries->htab);
		}
	}

	if (dentry)
	{
		r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry, &vd,
									false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif

		internal_cap_unlock(vol, dentry, vd);
	}

	RETURN_INT(r);
}

/*! Read COUNT bytes from offset OFFSET of local file DENTRY on volume VOL.
   Store data to BUFFER and count to RCOUNT.  */

static int32_t
local_read(read_res * res, internal_dentry dentry, uint64_t offset,
		   uint32_t count, volume vol)
{
	int32_t r;
	int fd;
	bool regular_file;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	regular_file = dentry->fh->attr.type == FT_REG;
	res->version = dentry->fh->attr.version;
	r = capability_open(&fd, 0, dentry, vol);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (regular_file || offset != (uint64_t) - 1)
	{
		r = lseek(fd, offset, SEEK_SET);
		if (r < 0)
		{
			zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
			RETURN_INT(errno);
		}
	}

	r = read(fd, res->data.buf, count);
	if (r < 0)
	{
		zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
		RETURN_INT(errno);
	}

	res->data.len = r;

	zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
	RETURN_INT(ZFS_OK);
}

/*! Read COUNT bytes from offset OFFSET of remote file with capability CAP of 
   dentry DENTRY on volume VOL.  */

static int32_t
remote_read(read_res * res, internal_cap cap, internal_dentry dentry,
			uint64_t offset, uint32_t count, volume vol)
{
	read_args args;
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;

	TRACE("");
	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_cap_undefined(cap->master_cap))
		zfsd_abort();
	if (zfs_fh_undefined(cap->master_cap.fh))
		zfsd_abort();
#endif

	args.cap = cap->master_cap;
	args.offset = offset;
	args.count = count;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_unlock(&vol->mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_read_client(t, &args, nod, &fd);

	if (r == ZFS_OK)
	{
		char *buffer = res->data.buf;

		if (!decode_read_res(t->dc_reply, res)
			|| !finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
		else
		{
			memcpy(buffer, res->data.buf, res->data.len);
			res->data.buf = buffer;
		}
	}
	else if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Read COUNT bytes from file CAP at offset OFFSET, store the results to
   RES. If UPDATE_LOCAL is true update the local file on copied volume.  */

int32_t
zfs_read(read_res * res, zfs_cap * cap, uint64_t offset, uint32_t count,
		 bool update_local)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	zfs_cap tmp_cap;
	int32_t r, r2;

	TRACE("offset = %" PRIu64 " count = %" PRIu32, offset, count);

	if (count > ZFS_MAXDATA)
		RETURN_INT(EINVAL);

	if (cap->flags != O_RDONLY && cap->flags != O_RDWR)
		RETURN_INT(EBADF);

	if (VIRTUAL_FH_P(cap->fh))
		RETURN_INT(EISDIR);

	r = validate_operation_on_zfs_fh(&cap->fh, EISDIR, EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	r = find_capability(cap, &icap, &vol, &dentry, NULL, true);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (dentry->fh->attr.type == FT_DIR)
	{
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(EISDIR);
	}

	r = internal_cap_lock(LEVEL_SHARED, &icap, &vol, &dentry, NULL, &tmp_cap);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
	{
		if (zfs_fh_undefined(dentry->fh->meta.master_fh)
			|| vol->master == this_node)
			r = local_read(res, dentry, offset, count, vol);
		else if (dentry->fh->attr.type == FT_REG && update_local)
		{
			varray blocks;
			uint64_t end;
			uint64_t offset2;
			uint32_t count2;
			unsigned int i;
			bool complete;

			message(LOG_FUNC, FACILITY_DATA,
					"zfs_read(): file has local path\n");

			count2 = (count < ZFS_UPDATED_BLOCK_SIZE
					  ? ZFS_UPDATED_BLOCK_SIZE : count);
			end = (offset < (uint64_t) - 1 - count2
				   ? offset + count2 : (uint64_t) - 1);

			message(LOG_FUNC, FACILITY_DATA,
					"zfs_read(): calling get_blocks_for_updating()\n");
			get_blocks_for_updating(dentry->fh, offset, end, &blocks);
			message(LOG_FUNC, FACILITY_DATA,
					"zfs_read(): back from get_blocks_for_updating()\n");

			complete = true;
			offset2 = offset + count;
			for (i = 0; i < VARRAY_USED(blocks); i++)
			{
				if (offset2 <= VARRAY_ACCESS(blocks, i, interval).start)
					break;

				if (VARRAY_ACCESS(blocks, i, interval).start <= offset
					&& offset < VARRAY_ACCESS(blocks, i, interval).end)
				{
					complete = false;
					break;
				}
				else if (VARRAY_ACCESS(blocks, i, interval).start < offset2
						 && offset2 <= VARRAY_ACCESS(blocks, i, interval).end)
				{
					complete = false;
					break;
				}
			}

			if (complete)
			{
				message(LOG_DEBUG, FACILITY_DATA,
						"zfs_read(): nothing to update\n");
				r = local_read(res, dentry, offset, count, vol);
			}
			else
			{
				message(LOG_DEBUG, FACILITY_DATA, "zfs_read(): will update\n");
				bool modified;

				if (icap->master_busy == 0)
				{
					r = cond_remote_open(&tmp_cap, icap, &dentry, &vol);
					if (r != ZFS_OK)
						goto out_update;

					icap->master_close_p = true;
				}

				modified = (dentry->fh->attr.version
							!= dentry->fh->meta.master_version);

				release_dentry(dentry);
				zfsd_mutex_unlock(&vol->mutex);
				zfsd_mutex_unlock(&fh_mutex);

				message(LOG_FUNC, FACILITY_DATA,
						"zfs_read(): calling update_file_blocks\n");

				/* update the file blocks needed for this read, parameter for
				   slow = false, we don't want to get interrupted here, it's
				   not background update */
				r = update_file_blocks(&tmp_cap, &blocks, modified, false);
				if (r == ZFS_OK)
				{
				  out_update:
					r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry,
												NULL, false);
#ifdef ENABLE_CHECKING
					if (r2 != ZFS_OK)
						zfsd_abort();
#endif

					r = local_read(res, dentry, offset, count, vol);
				}
			}

			varray_destroy(&blocks);
		}
		else
		{
			switch (dentry->fh->attr.type)
			{
			case FT_REG:
				r = local_read(res, dentry, offset, count, vol);
				break;

			case FT_BLK:
			case FT_CHR:
			case FT_SOCK:
			case FT_FIFO:
				if (!zfs_cap_undefined(icap->master_cap))
				{
					zfsd_mutex_unlock(&fh_mutex);
					r = remote_read(res, icap, dentry, offset, count, vol);
				}
				else
					r = local_read(res, dentry, offset, count, vol);
				break;

			default:
				zfsd_abort();
			}
		}
	}
	else if (vol->master != this_node)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_read(res, icap, dentry, offset, count, vol);
	}
	else
		zfsd_abort();

	r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

#ifdef ENABLE_VERSIONS
	if (zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG) & (r == ZFS_OK)
		&& (dentry->version_file) && (offset < dentry->fh->attr.size)
		&& (dentry->fh->version_list_length))
	{
		r = version_read_old_data(dentry, offset, offset + count,
								  res->data.buf);
	}
#endif

	internal_cap_unlock(vol, dentry, NULL);

	RETURN_INT(r);
}

/*! Write DATA to offset OFFSET of local file DENTRY on volume VOL.  */

static int32_t
local_write(write_res * res, internal_dentry dentry,
			uint64_t offset, data_buffer * data, volume vol,
			ATTRIBUTE_UNUSED_VERSIONS bool remote)
{
	int32_t r;
	off_t writing_position = -1;
	int fd;
#ifdef ENABLE_VERSIONS
	bool version_was_open = true;
	bool version_write = false;
	int fdv = -1;
	varray save;
	unsigned int i;
	uint32_t verend;
#endif

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

#ifdef ENABLE_VERSIONS
	if (!remote && zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG))
	{
		// we have to store original data prior its modification
		if (!WAS_FILE_TRUNCATED(dentry->fh))
		{
			// version file open?
			if (dentry->fh->version_fd < 0)
			{
				version_create_file(dentry, vol);
				version_was_open = false;
			}

			if (1)
			{
				// write before marked file size
				version_write = true;
				fdv = dentry->fh->version_fd;
				verend = offset + data->len;
				// if (verend > dentry->fh->marked_size) verend =
				// dentry->fh->marked_size;

				// get intervals that should be copied
				interval_tree_complement(dentry->fh->versioned, offset, verend,
										 &save);

				// write our new interval into tree
				interval_tree_insert(dentry->fh->versioned, offset, verend);
			}
		}
	}
#endif

	r = capability_open(&fd, 0, dentry, vol);
	if (r != ZFS_OK)
	{
#ifdef ENABLE_VERSIONS
		// TODO: should not use fh - not locked here
		if (!remote && zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG)
			&& (dentry->fh->version_fd > 0))
			version_close_file(dentry->fh, false);
#endif
		RETURN_INT(r);
	}

#ifdef ENABLE_VERSIONS
	if (!remote && zfs_config.versions.versioning && (dentry->fh->attr.type == FT_REG)
		&& version_write)
	{
		for (i = 0; i < VARRAY_USED(save); i++)
		{
			interval *x;

			x = &VARRAY_ACCESS(save, i, interval);
			version_copy_data(fd, fdv, x->start, x->end - x->start, data);
		}
		varray_destroy(&save);
	}
#endif

	writing_position = lseek(fd, offset, SEEK_SET);
	if (writing_position == (off_t) - 1)
	{
		zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
		RETURN_INT(errno);
	}

	message(LOG_DEBUG, FACILITY_DATA,
			"writing data of size %u to %ld(wanted %llu - %ld)\n",
			data->len, writing_position, offset, (long int)offset);

	r = write(fd, data->buf, data->len);
	if (r < 0)
	{
		zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
		RETURN_INT(errno);
	}
	res->written = r;
	message(LOG_DEBUG, FACILITY_DATA,
			"written %d of %u, pos is %ld\n",
			r, data->len, lseek(fd, 0, SEEK_CUR));

	zfsd_mutex_unlock(&internal_fd_data[fd].mutex);
	RETURN_INT(ZFS_OK);
}

/*! Write to remote file with capability CAP of dentry DENTRY on volume VOL. */

static int32_t
remote_write(write_res * res, internal_cap cap, internal_dentry dentry,
			 write_args * args, volume vol)
{
	thread *t;
	int32_t r;
	int fd;
	node nod = vol->master;

	TRACE("");

	CHECK_MUTEX_LOCKED(&vol->mutex);
#ifdef ENABLE_CHECKING
	if (zfs_cap_undefined(cap->master_cap))
		zfsd_abort();
	if (zfs_fh_undefined(cap->master_cap.fh))
		zfsd_abort();
#endif

	args->cap = cap->master_cap;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_unlock(&vol->mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_write_client(t, args, nod, &fd);

	if (r == ZFS_OK)
	{
		if (!decode_write_res(t->dc_reply, res)
			|| !finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}
	else if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);
	RETURN_INT(r);
}

/*! Write to file.  */

int32_t zfs_write(write_res * res, write_args * args)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	zfs_cap tmp_cap;
	int32_t r, r2;
	bool remote_call = false;

	TRACE("");

	if (args->data.len > ZFS_MAXDATA)
		RETURN_INT(EINVAL);

	if (args->cap.flags != O_WRONLY && args->cap.flags != O_RDWR)
		RETURN_INT(EBADF);

	if (VIRTUAL_FH_P(args->cap.fh))
		RETURN_INT(EISDIR);

	r = validate_operation_on_zfs_fh(&args->cap.fh, EINVAL, EINVAL);
	if (r != ZFS_OK)
		RETURN_INT(r);

	r = find_capability(&args->cap, &icap, &vol, &dentry, NULL, true);
	if (r != ZFS_OK)
		RETURN_INT(r);

#ifdef ENABLE_CHECKING
	/* We did not allow directory to be opened for writing so there should be
	   no capability for writing to directory.  */
	if (dentry->fh->attr.type == FT_DIR)
		zfsd_abort();
#endif

	r = internal_cap_lock(LEVEL_SHARED, &icap, &vol, &dentry, NULL, &tmp_cap);
	if (r != ZFS_OK)
		RETURN_INT(r);

	if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh))
	{
		if (zfs_fh_undefined(dentry->fh->meta.master_fh)
			|| vol->master == this_node)
			r = local_write(res, dentry, args->offset, &args->data, vol,
							args->remote);
		else
		{
			switch (dentry->fh->attr.type)
			{
			case FT_REG:
				r = local_write(res, dentry, args->offset, &args->data, vol,
								args->remote);
				break;

			case FT_BLK:
			case FT_CHR:
			case FT_SOCK:
			case FT_FIFO:
				if (!zfs_cap_undefined(icap->master_cap))
				{
					zfsd_mutex_unlock(&fh_mutex);
					r = remote_write(res, icap, dentry, args, vol);
					remote_call = true;
				}
				else
					r = local_write(res, dentry, args->offset, &args->data,
									vol, args->remote);
				break;

			default:
				zfsd_abort();
			}
		}
	}
	else if (vol->master != this_node)
	{
		zfsd_mutex_unlock(&fh_mutex);
		r = remote_write(res, icap, dentry, args, vol);
		remote_call = true;
	}
	else
		zfsd_abort();

	r2 = find_capability_nolock(&tmp_cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	if (r == ZFS_OK)
	{
		if (INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			&& dentry->fh->attr.type == FT_REG)
		{
			if (vol->master == this_node)
			{
				TRACE("increasing version on master");
				if (!inc_local_version(vol, dentry->fh))
					MARK_VOLUME_DELETE(vol);
			}
			else
			{
				uint64_t start, end;
				varray blocks;
				unsigned int i;

				if (!inc_local_version_and_modified(vol, dentry->fh))
					MARK_VOLUME_DELETE(vol);

				start = (args->offset / ZFS_MODIFIED_BLOCK_SIZE
						 * ZFS_MODIFIED_BLOCK_SIZE);
				end = ((args->offset + res->written
						+ ZFS_MODIFIED_BLOCK_SIZE - 1)
					   / ZFS_MODIFIED_BLOCK_SIZE * ZFS_MODIFIED_BLOCK_SIZE);

				interval_tree_intersection(dentry->fh->updated, start, end,
										   &blocks);

				start = args->offset;
				if (dentry->fh->attr.size < start)
					start = dentry->fh->attr.size;
				end = args->offset + res->written;
				if (dentry->fh->attr.size < end)
					dentry->fh->attr.size = end;
				for (i = 0; i < VARRAY_USED(blocks); i++)
				{
					if (VARRAY_ACCESS(blocks, i, interval).end < start)
						continue;
					if (VARRAY_ACCESS(blocks, i, interval).start > end)
						break;

					/* Now the interval is joinable with [START, END).  */
					if (VARRAY_ACCESS(blocks, i, interval).start < start)
						start = VARRAY_ACCESS(blocks, i, interval).start;
					if (VARRAY_ACCESS(blocks, i, interval).end > end)
						end = VARRAY_ACCESS(blocks, i, interval).end;
				}

				if (!append_interval(vol, dentry->fh,
									 METADATA_TYPE_UPDATED, start, end))
					MARK_VOLUME_DELETE(vol);
				if (!append_interval(vol, dentry->fh,
									 METADATA_TYPE_MODIFIED, start, end))
					MARK_VOLUME_DELETE(vol);

				varray_destroy(&blocks);
			}
		}

		if (!remote_call)
		{
			/* Version of remote files is already initialized when decoding
			   reply of remote call.  */
			res->version = dentry->fh->attr.version;
		}
	}

	internal_cap_unlock(vol, dentry, NULL);

	RETURN_INT(r);
}

/*! Read complete contents of local directory FH and store it to ENTRIES.  */

int32_t full_local_readdir(zfs_fh * fh, filldir_htab_entries * entries)
{
	int32_t r, r2;
	zfs_cap cap;
	internal_dentry dentry;
	internal_cap icap;
	volume vol;
	dir_list list;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (!REGULAR_FH_P(*fh))
		zfsd_abort();
#endif

	cap.fh = *fh;
	cap.flags = O_RDONLY;

	/* Open directory.  */
	r2 = get_capability(&cap, &icap, &vol, &dentry, NULL, false, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	r = local_open(0, dentry, vol);

	r2 = find_capability_nolock(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	if (r != ZFS_OK)
	{
		put_capability(icap, dentry->fh, NULL);
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		zfsd_mutex_unlock(&fh_mutex);
		RETURN_INT(r);
	}

	/* Read directory.  */
	entries->htab = htab_create(32, filldir_htab_hash,
								filldir_htab_eq, filldir_htab_del, NULL);
	entries->last_cookie = 0;

	do
	{
		list.n = 0;
		list.eof = false;
		list.buffer = entries;
		r = local_readdir(&list, dentry, NULL, fh, entries->last_cookie,
						  NULL, vol, &filldir_htab, false);
		if (r != ZFS_OK)
		{
			r2 = find_capability(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
				zfsd_abort();
#endif
			local_close(dentry->fh);
			put_capability(icap, dentry->fh, NULL);
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			RETURN_INT(r);
		}

		r2 = find_capability_nolock(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif
	}
	while (list.eof == 0);

	/* Close directory.  */
	zfsd_mutex_unlock(&vol->mutex);
	zfsd_mutex_unlock(&fh_mutex);
	r = local_close(dentry->fh);
	put_capability(icap, dentry->fh, NULL);
	release_dentry(dentry);
	RETURN_INT(r);
}

/*! Read complete contents of remote directory FH and store it to ENTRIES.  */

int32_t full_remote_readdir(zfs_fh * fh, filldir_htab_entries * entries)
{
	int32_t r, r2;
	zfs_cap cap;
	internal_dentry dentry;
	internal_cap icap;
	volume vol;
	dir_list list;
	readdir_data data;
	zfs_cap remote_cap;

	TRACE("");
#ifdef ENABLE_CHECKING
	if (!REGULAR_FH_P(*fh))
		zfsd_abort();
#endif

	cap.fh = *fh;
	cap.flags = O_RDONLY;

	/* Open directory.  */
	r2 = get_capability(&cap, &icap, &vol, &dentry, NULL, true, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	r = remote_open(&remote_cap, icap, 0, dentry, vol);

	r2 = find_capability(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	if (r != ZFS_OK)
	{
		put_capability(icap, dentry->fh, NULL);
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(r);
	}
	icap->master_cap = remote_cap;

	/* Read directory.  */
	entries->htab = htab_create(32, filldir_htab_hash,
								filldir_htab_eq, filldir_htab_del, NULL);
	entries->last_cookie = 0;

	do
	{
		list.n = 0;
		list.eof = false;
		list.buffer = entries;
		data.written = 0;
		data.count = ZFS_MAXDATA;
		r = remote_readdir(&list, icap, dentry, entries->last_cookie,
						   &data, vol, &filldir_htab);

		r2 = find_capability(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
#endif

		if (r != ZFS_OK)
		{
			remote_close(icap, dentry, vol);

			r2 = find_capability(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
			if (r2 != ZFS_OK)
				zfsd_abort();
#endif

			put_capability(icap, dentry->fh, NULL);
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			RETURN_INT(r);
		}
	}
	while (list.eof == 0);

	/* Close directory.  */
	r = remote_close(icap, dentry, vol);

	r2 = find_capability(&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r2 != ZFS_OK)
		zfsd_abort();
#endif

	put_capability(icap, dentry->fh, NULL);
	release_dentry(dentry);
	zfsd_mutex_unlock(&vol->mutex);
	RETURN_INT(r);
}

/*! Read as many bytes as possible of block of local file CAP starting at
   OFFSET which is COUNT bytes long, store the data to BUFFER and the number
   of bytes read to RCOUNT.  */

int32_t
full_local_read(uint32_t * rcount, void *buffer, zfs_cap * cap,
				uint64_t offset, uint32_t count, uint64_t * version)
{
	read_res res;
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	uint32_t total;
	int32_t r;

	TRACE("");

	for (total = 0; total < count; total += res.data.len)
	{
		r = find_capability_nolock(cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r != ZFS_OK)
			zfsd_abort();
#endif

#ifdef ENABLE_CHECKING
		if (!(INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			  && vol->master != this_node))
			zfsd_abort();
#endif

		if (version && *version != dentry->fh->attr.version)
		{
			*version = dentry->fh->attr.version;
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			zfsd_mutex_unlock(&fh_mutex);
			RETURN_INT(ZFS_CHANGED);
		}

		res.data.buf = (char *)buffer + total;
		r = local_read(&res, dentry, offset + total, count - total, vol);
		if (r != ZFS_OK)
			RETURN_INT(r);

		if (res.data.len == 0)
			break;
	}

	*rcount = total;
	RETURN_INT(ZFS_OK);
}

/*! Read as many bytes as possible of block of local file DENTRY with
   capability CAP on volume VOL starting at OFFSET which is COUNT bytes long,
   store the data to BUFFER and the number of bytes read to RCOUNT.  */

int32_t
full_local_read_dentry(uint32_t * rcount, void *buffer, zfs_cap * cap,
					   internal_dentry dentry, volume vol, uint64_t offset,
					   uint32_t count)
{
	read_res res;
	internal_cap icap;
	uint32_t total;
	int32_t r, r2;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	for (total = 0; total < count; total += res.data.len)
	{
		res.data.buf = (char *)buffer + total;
		r = local_read(&res, dentry, offset + total, count - total, vol);

		r2 = find_capability_nolock(cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
		if (!(INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			  && vol->master != this_node))
			zfsd_abort();
#endif

		if (r != ZFS_OK)
			RETURN_INT(r);

		if (res.data.len == 0)
			break;
	}

	*rcount = total;
	RETURN_INT(ZFS_OK);
}

/*! Read as many bytes as possible of block of remote file CAP starting at
   OFFSET which is COUNT bytes long, store the data to BUFFER and the number
   of bytes read to RCOUNT.  */

int32_t
full_remote_read(uint32_t * rcount, void *buffer, zfs_cap * cap,
				 uint64_t offset, uint32_t count, uint64_t * version)
{
	read_res res;
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	uint32_t total;
	int32_t r;

	TRACE("");

	for (total = 0; total < count; total += res.data.len)
	{
		r = find_capability(cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r != ZFS_OK)
			zfsd_abort();
#endif

#ifdef ENABLE_CHECKING
		if (!(INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			  && vol->master != this_node))
			zfsd_abort();
#endif

		res.data.buf = (char *)buffer + total;
		r = remote_read(&res, icap, dentry, offset + total, count - total,
						vol);
		if (r != ZFS_OK)
			RETURN_INT(r);
		if (version && res.version != *version)
		{
			*version = res.version;
			RETURN_INT(ZFS_CHANGED);
		}

		if (res.data.len == 0)
			break;
	}

	*rcount = total;
	RETURN_INT(ZFS_OK);
}

/*! Write as many bytes as possible from BUFFER of length COUNT to local file
   CAP starting at OFFSET.  Store the number of bytes read to RCOUNT.  */

int32_t
full_local_write(uint32_t * rcount, void *buffer, zfs_cap * cap,
				 uint64_t offset, uint32_t count, uint64_t * version)
{
	volume vol;
	internal_cap icap;
	internal_dentry dentry;
	data_buffer data;
	write_res res;
	uint32_t total;
	int32_t r;

	TRACE("");

	for (total = 0; total < count; total += res.written)
	{
		r = find_capability_nolock(cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r != ZFS_OK)
			zfsd_abort();
#endif

#ifdef ENABLE_CHECKING
		if (!(INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			  && vol->master != this_node))
			zfsd_abort();
#endif

		if (version && *version != dentry->fh->attr.version)
		{
			*version = dentry->fh->attr.version;
			release_dentry(dentry);
			zfsd_mutex_unlock(&vol->mutex);
			zfsd_mutex_unlock(&fh_mutex);
			RETURN_INT(ZFS_CHANGED);
		}

		data.len = count - total;
		data.buf = (char *)buffer + total;
		r = local_write(&res, dentry, offset + total, &data, vol, false);
		if (r != ZFS_OK)
			RETURN_INT(r);

		if (res.written == 0)
			break;
	}

	*rcount = total;
	RETURN_INT(ZFS_OK);
}

/*! Write as many bytes as possible from BUFFER of length COUNT to remote
   file DENTRY with capability CAP and ICAP on volume VOL starting at OFFSET.
   Store the number of bytes read to RCOUNT.  */

int32_t
full_remote_write_dentry(uint32_t * rcount, void *buffer, zfs_cap * cap,
						 internal_cap icap, internal_dentry dentry,
						 volume vol, uint64_t offset, uint32_t count,
						 uint64_t * version_increase)
{
	write_args args;
	write_res res;
	uint32_t total;
	int32_t r, r2;

	TRACE("");
	CHECK_MUTEX_LOCKED(&fh_mutex);
	CHECK_MUTEX_LOCKED(&vol->mutex);
	CHECK_MUTEX_LOCKED(&dentry->fh->mutex);

	for (total = 0; total < count; total += res.written)
	{
		zfsd_mutex_unlock(&fh_mutex);
		args.offset = offset + total;
		args.data.len = count - total;
		args.data.buf = (char *)buffer + total;
		r = remote_write(&res, icap, dentry, &args, vol);

		r2 = find_capability_nolock(cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
		if (r2 != ZFS_OK)
			zfsd_abort();
		if (!(INTERNAL_FH_HAS_LOCAL_PATH(dentry->fh)
			  && vol->master != this_node))
			zfsd_abort();
#endif

		if (r != ZFS_OK)
			RETURN_INT(r);

		if (res.written == 0)
			break;

		(*version_increase)++;
	}

	*rcount = total;
	RETURN_INT(ZFS_OK);
}

/*! Compute MD5 sum for ARGS->COUNT ranges starting at ARGS->OFFSET[i] with
   length ARGS->LENGTH[i] of local file ARGS->CAP and store them (together
   with the information about ranges) to RES.  */

int32_t local_md5sum(md5sum_res * res, md5sum_args * args)
{
	read_res rres;
	internal_dentry dentry;
	uint32_t i;
	MD5Context context;
	unsigned char buf[ZFS_MAXDATA];
	int32_t r;
	uint32_t total;

	TRACE("");

	zfsd_mutex_lock(&fh_mutex);
	dentry = dentry_lookup(&args->cap.fh);
	zfsd_mutex_unlock(&fh_mutex);
	if (!dentry)
		RETURN_INT(ZFS_STALE);

	res->count = 0;
	res->size = dentry->fh->attr.size;
	res->version = dentry->fh->attr.version;
	release_dentry(dentry);

	rres.data.buf = (char *)buf;
	for (i = 0; i < args->count; i++)
	{
		MD5Init(&context);
		for (total = 0; total < args->length[i]; total += rres.data.len)
		{
			r = zfs_read(&rres, &args->cap, args->offset[i] + total,
						 args->length[i] - total, false);
			if (r != ZFS_OK)
				RETURN_INT(r);
			if (!args->ignore_changes && rres.version != res->version)
				RETURN_INT(ZFS_CHANGED);

			if (rres.data.len == 0)
				break;

			MD5Update(&context, buf, rres.data.len);
		}

		if (total > 0)
		{
			res->offset[res->count] = args->offset[i];
			res->length[res->count] = total;
			MD5Final(res->md5sum[res->count], &context);
			res->count++;
		}
	}

	RETURN_INT(ZFS_OK);
}

/*! Compute MD5 sum for ARGS->COUNT ranges starting at ARGS->OFFSET[i] with
   length ARGS->LENGTH[i] of remote file ARGS->CAP and store them (together
   with the information about ranges) to RES.  */

int32_t remote_md5sum(md5sum_res * res, md5sum_args * args)
{
	volume vol;
	node nod;
	internal_cap icap;
	internal_dentry dentry;
	thread *t;
	int32_t r;
	int fd;

	TRACE("");

	r = find_capability(&args->cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	if (r != ZFS_OK)
		zfsd_abort();
#endif

#ifdef ENABLE_CHECKING
	if (zfs_cap_undefined(icap->master_cap))
		zfsd_abort();
	if (zfs_fh_undefined(icap->master_cap.fh))
		zfsd_abort();
#endif

	if (dentry->fh->attr.type != FT_REG)
	{
		release_dentry(dentry);
		zfsd_mutex_unlock(&vol->mutex);
		RETURN_INT(EINVAL);
	}

	nod = vol->master;
	args->cap = icap->master_cap;

	release_dentry(dentry);
	zfsd_mutex_lock(&node_mutex);
	zfsd_mutex_lock(&nod->mutex);
	zfsd_mutex_unlock(&node_mutex);
	zfsd_mutex_unlock(&vol->mutex);

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_md5sum_client(t, args, nod, &fd);

	if (r == ZFS_OK)
	{
		if (!decode_md5sum_res(t->dc_reply, res)
			|| !finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}
	else if (r >= ZFS_LAST_DECODED_ERROR)
	{
		if (!finish_decoding(t->dc_reply))
			r = ZFS_INVALID_REPLY;
	}

	if (r >= ZFS_ERROR_HAS_DC_REPLY)
		recycle_dc_to_fd(t->dc_reply, fd);

	RETURN_INT(r);
}

/*! Reread remote config file PATH (relative path WRT volume root).  */

void remote_reread_config(string * path, node nod)
{
	reread_config_args args;
	thread *t;
	int32_t r;
	int fd;

	TRACE("");
	CHECK_MUTEX_LOCKED(&nod->mutex);

	args.path = *path;

	t = (thread *) pthread_getspecific(thread_data_key);
	r = zfs_proc_reread_config_client(t, &args, nod, &fd);

	RETURN_VOID;
}

/*! Initialize data structures in FILE.C.  */

void initialize_file_c(void)
{
	int i;

	zfsd_mutex_init(&opened_mutex);
	opened = fibheap_new(max_local_fds, &opened_mutex);

	zfsd_mutex_init(&dir_entry_mutex);
	dir_entry_pool = create_alloc_pool("dir_entry", sizeof(dir_entry), 1020,
									   &dir_entry_mutex);

	/* Data for each file descriptor.  */
	internal_fd_data
		= (internal_fd_data_t *) xcalloc(max_nfd, sizeof(internal_fd_data_t));
	for (i = 0; i < max_nfd; i++)
	{
		zfsd_mutex_init(&internal_fd_data[i].mutex);
		internal_fd_data[i].fd = -1;
	}
}

/*! Destroy data structures in CAP.C.  */

void cleanup_file_c(void)
{
	while (fibheap_size(opened) > 0)
	{
		internal_fd_data_t *fd_data;

		zfsd_mutex_lock(&opened_mutex);
		fd_data = (internal_fd_data_t *) fibheap_extract_min(opened);
#ifdef ENABLE_CHECKING
		if (!fd_data && fibheap_size(opened) > 0)
			zfsd_abort();
#endif
		if (fd_data)
		{
			zfsd_mutex_lock(&fd_data->mutex);
			fd_data->heap_node = NULL;
			if (fd_data->fd >= 0)
				close_local_fd(fd_data->fd);
			else
				zfsd_mutex_unlock(&fd_data->mutex);
		}
		zfsd_mutex_unlock(&opened_mutex);
	}

	zfsd_mutex_lock(&dir_entry_mutex);
#ifdef ENABLE_CHECKING
	if (dir_entry_pool->elts_free < dir_entry_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_MEMORY,
				"Memory leak (%u elements) in dir_entry_pool.\n",
				dir_entry_pool->elts_allocated - dir_entry_pool->elts_free);
#endif
	free_alloc_pool(dir_entry_pool);
	zfsd_mutex_unlock(&dir_entry_mutex);
	zfsd_mutex_destroy(&dir_entry_mutex);

	zfsd_mutex_lock(&opened_mutex);
	fibheap_delete(opened);
	zfsd_mutex_unlock(&opened_mutex);
	zfsd_mutex_destroy(&opened_mutex);

	free(internal_fd_data);
}
