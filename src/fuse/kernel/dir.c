/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2006  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "fuse_i.h"

#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/namei.h>

#if BITS_PER_LONG >= 64
static inline void fuse_dentry_settime(struct dentry *entry, u64 time)
{
	entry->d_time = time;
}

static inline u64 fuse_dentry_time(struct dentry *entry)
{
	return entry->d_time;
}
#else
/*
 * On 32 bit archs store the high 32 bits of time in d_fsdata
 */
static void fuse_dentry_settime(struct dentry *entry, u64 time)
{
	entry->d_time = time;
	entry->d_fsdata = (void *) (unsigned long) (time >> 32);
}

static u64 fuse_dentry_time(struct dentry *entry)
{
	return (u64) entry->d_time +
		((u64) (unsigned long) entry->d_fsdata << 32);
}
#endif

/*
 * FUSE caches dentries and attributes with separate timeout.  The
 * time in jiffies until the dentry/attributes are valid is stored in
 * dentry->d_time and fuse_inode->i_time respectively.
 */

/*
 * Calculate the time in jiffies until a dentry/attributes are valid
 */
static u64 time_to_jiffies(unsigned long sec, unsigned long nsec)
{
	if (sec || nsec) {
		struct timespec ts = {sec, nsec};
		return get_jiffies_64() + timespec_to_jiffies(&ts);
	} else
		return 0;
}

/*
 * Set dentry and possibly attribute timeouts from the lookup/mk*
 * replies
 */
static void fuse_change_timeout(struct dentry *entry, struct fuse_entry_out *o)
{
	fuse_dentry_settime(entry,
		time_to_jiffies(o->entry_valid, o->entry_valid_nsec));
	if (entry->d_inode)
		get_fuse_inode(entry->d_inode)->i_time =
			time_to_jiffies(o->attr_valid, o->attr_valid_nsec);
}

/*
 * Mark the attributes as stale, so that at the next call to
 * ->getattr() they will be fetched from userspace
 */
void fuse_invalidate_attr(struct inode *inode)
{
	get_fuse_inode(inode)->i_time = 0;
}

/*
 * Just mark the entry as stale, so that a next attempt to look it up
 * will result in a new lookup call to userspace
 *
 * This is called when a dentry is about to become negative and the
 * timeout is unknown (unlink, rmdir, rename and in some cases
 * lookup)
 */
static void fuse_invalidate_entry_cache(struct dentry *entry)
{
	fuse_dentry_settime(entry, 0);
}

/*
 * Same as fuse_invalidate_entry_cache(), but also try to remove the
 * dentry from the hash
 */
static void fuse_invalidate_entry(struct dentry *entry)
{
	d_invalidate(entry);
	fuse_invalidate_entry_cache(entry);
}

static void fuse_lookup_init(struct fuse_req *req, struct inode *dir,
			     struct dentry *entry,
			     struct fuse_entry_out *outarg)
{
	req->in.h.opcode = FUSE_LOOKUP;
	req->in.h.nodeid = get_node_id(dir);
	req->in.numargs = 1;
	req->in.args[0].size = entry->d_name.len + 1;
	req->in.args[0].value = entry->d_name.name;
	req->out.numargs = 1;
	req->out.args[0].size = sizeof(struct fuse_entry_out);
	req->out.args[0].value = outarg;
}

/*
 * Check whether the dentry is still valid
 *
 * If the entry validity timeout has expired and the dentry is
 * positive, try to redo the lookup.  If the lookup results in a
 * different inode, then let the VFS invalidate the dentry and redo
 * the lookup once more.  If the lookup results in the same inode,
 * then refresh the attributes, timeouts and mark the dentry valid.
 */
static int fuse_dentry_revalidate(struct dentry *entry, struct nameidata *nd)
{
	struct inode *inode = entry->d_inode;
	int invalid;

	if (inode && is_bad_inode(inode))
		return 0;
	invalid = 0;
	if (inode && get_fuse_inode(inode)->i_time == (u64)-1) {
		/* The dentry was in use when the inode purge request was
		   processed. */
		shrink_dcache_parent(entry);
		d_drop(entry);
		invalid = 1;
	} else if (fuse_dentry_time(entry) < get_jiffies_64())
		invalid = 1;
	if (invalid) {
		int err;
		struct fuse_entry_out outarg;
		struct fuse_conn *fc;
		struct fuse_req *req;
		struct fuse_req *forget_req;
		struct dentry *parent;

		/* For negative dentries, always do a fresh lookup */
		if (!inode)
			return 0;

		fc = get_fuse_conn(inode);
		req = fuse_get_req(fc);
		if (IS_ERR(req))
			return 0;

		forget_req = fuse_get_req(fc);
		if (IS_ERR(forget_req)) {
			fuse_put_request(fc, req);
			return 0;
		}

		parent = dget_parent(entry);
		fuse_lookup_init(req, parent->d_inode, entry, &outarg);
		request_send(fc, req);
		dput(parent);
		err = req->out.h.error;
		fuse_put_request(fc, req);
		/* Zero nodeid is same as -ENOENT */
		if (!err && !outarg.nodeid)
			err = -ENOENT;
		if (!err) {
			struct fuse_inode *fi = get_fuse_inode(inode);
			if (outarg.nodeid != get_node_id(inode)) {
				fuse_send_forget(fc, forget_req,
						 outarg.nodeid, 1);
				return 0;
			}
			spin_lock(&fc->lock);
			fi->nlookup ++;
			spin_unlock(&fc->lock);
		}
		fuse_put_request(fc, forget_req);
		if (err || (outarg.attr.mode ^ inode->i_mode) & S_IFMT)
			return 0;

		fuse_change_attributes(inode, &outarg.attr);
		fuse_change_timeout(entry, &outarg);
	}
	return 1;
}

static int invalid_nodeid(u64 nodeid)
{
	return !nodeid || nodeid == FUSE_ROOT_ID;
}

static struct dentry_operations fuse_dentry_operations = {
	.d_revalidate	= fuse_dentry_revalidate,
};

static int valid_mode(int m)
{
	return S_ISREG(m) || S_ISDIR(m) || S_ISLNK(m) || S_ISCHR(m) ||
		S_ISBLK(m) || S_ISFIFO(m) || S_ISSOCK(m);
}

/*
 * Add a directory inode to a dentry, ensuring that no other dentry
 * refers to this inode.  Called with fc->inst_mutex.
 */
static struct dentry *fuse_d_add_directory(struct dentry *entry,
					   struct inode *inode)
{
	struct dentry *alias = d_find_alias(inode);
	if (alias && !(alias->d_flags & DCACHE_DISCONNECTED)) {
		/* This tries to shrink the subtree below alias */
		fuse_invalidate_entry(alias);
		dput(alias);
		if (!list_empty(&inode->i_dentry))
			return ERR_PTR(-EBUSY);
	} else
		dput(alias);
	return d_splice_alias(inode, entry);
}

static struct dentry *fuse_lookup(struct inode *dir, struct dentry *entry,
				  struct nameidata *nd)
{
	int err;
	struct fuse_entry_out outarg;
	struct inode *inode = NULL;
	struct dentry *newent;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req;
	struct fuse_req *forget_req;

	if (entry->d_name.len > FUSE_NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return ERR_PTR(PTR_ERR(req));

	forget_req = fuse_get_req(fc);
	if (IS_ERR(forget_req)) {
		fuse_put_request(fc, req);
		return ERR_PTR(PTR_ERR(forget_req));
	}

	fuse_lookup_init(req, dir, entry, &outarg);
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	/* Zero nodeid is same as -ENOENT, but with valid timeout */
	if (!err && outarg.nodeid &&
	    (invalid_nodeid(outarg.nodeid) || !valid_mode(outarg.attr.mode)))
		err = -EIO;
	if (!err && outarg.nodeid) {
		inode = fuse_iget(dir->i_sb, outarg.nodeid, outarg.generation,
				  &outarg.attr);
		if (!inode) {
			fuse_send_forget(fc, forget_req, outarg.nodeid, 1);
			return ERR_PTR(-ENOMEM);
		}
	}
	fuse_put_request(fc, forget_req);
	if (err && err != -ENOENT)
		return ERR_PTR(err);

	if (inode && S_ISDIR(inode->i_mode)) {
		mutex_lock(&fc->inst_mutex);
		newent = fuse_d_add_directory(entry, inode);
		mutex_unlock(&fc->inst_mutex);
		if (IS_ERR(newent)) {
			iput(inode);
			return newent;
		}
	} else
		newent = d_splice_alias(inode, entry);

	entry = newent ? newent : entry;
	entry->d_op = &fuse_dentry_operations;
	if (!err)
		fuse_change_timeout(entry, &outarg);
	else
		fuse_invalidate_entry_cache(entry);
	return newent;
}

#ifdef HAVE_LOOKUP_INSTANTIATE_FILP
/*
 * Synchronous release for the case when something goes wrong in CREATE_OPEN
 */
static void fuse_sync_release(struct fuse_conn *fc, struct fuse_file *ff,
			      u64 nodeid, int flags)
{
	struct fuse_req *req;

	req = fuse_release_fill(ff, nodeid, flags, FUSE_RELEASE);
	req->force = 1;
	request_send(fc, req);
	fuse_put_request(fc, req);
}

/*
 * Atomic create+open operation
 *
 * If the filesystem doesn't support this, then fall back to separate
 * 'mknod' + 'open' requests.
 */
static int fuse_create_open(struct inode *dir, struct dentry *entry, int mode,
			    struct nameidata *nd)
{
	int err;
	struct inode *inode;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req;
	struct fuse_req *forget_req;
	struct fuse_open_in inarg;
	struct fuse_open_out outopen;
	struct fuse_entry_out outentry;
	struct fuse_file *ff;
	struct file *file;
	int flags = nd->intent.open.flags - 1;

	if (fc->no_create)
		return -ENOSYS;

	forget_req = fuse_get_req(fc);
	if (IS_ERR(forget_req))
		return PTR_ERR(forget_req);

	req = fuse_get_req(fc);
	err = PTR_ERR(req);
	if (IS_ERR(req))
		goto out_put_forget_req;

	err = -ENOMEM;
	ff = fuse_file_alloc();
	if (!ff)
		goto out_put_request;

	flags &= ~O_NOCTTY;
	memset(&inarg, 0, sizeof(inarg));
	inarg.flags = flags;
	inarg.mode = mode;
	req->in.h.opcode = FUSE_CREATE;
	req->in.h.nodeid = get_node_id(dir);
	req->in.numargs = 2;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = entry->d_name.len + 1;
	req->in.args[1].value = entry->d_name.name;
	req->out.numargs = 2;
	req->out.args[0].size = sizeof(outentry);
	req->out.args[0].value = &outentry;
	req->out.args[1].size = sizeof(outopen);
	req->out.args[1].value = &outopen;
	request_send(fc, req);
	err = req->out.h.error;
	if (err) {
		if (err == -ENOSYS)
			fc->no_create = 1;
		goto out_free_ff;
	}

	err = -EIO;
	if (!S_ISREG(outentry.attr.mode) || invalid_nodeid(outentry.nodeid))
		goto out_free_ff;

	fuse_put_request(fc, req);
	inode = fuse_iget(dir->i_sb, outentry.nodeid, outentry.generation,
			  &outentry.attr);
	if (!inode) {
		flags &= ~(O_CREAT | O_EXCL | O_TRUNC);
		ff->fh = outopen.fh;
		fuse_sync_release(fc, ff, outentry.nodeid, flags);
		fuse_send_forget(fc, forget_req, outentry.nodeid, 1);
		return -ENOMEM;
	}
	fuse_put_request(fc, forget_req);
	d_instantiate(entry, inode);
	fuse_change_timeout(entry, &outentry);
	file = lookup_instantiate_filp(nd, entry, generic_file_open);
	if (IS_ERR(file)) {
		ff->fh = outopen.fh;
		fuse_sync_release(fc, ff, outentry.nodeid, flags);
		return PTR_ERR(file);
	}
	fuse_finish_open(inode, file, ff, &outopen);
	return 0;

 out_free_ff:
	fuse_file_free(ff);
 out_put_request:
	fuse_put_request(fc, req);
 out_put_forget_req:
	fuse_put_request(fc, forget_req);
	return err;
}
#endif

/*
 * Code shared between mknod, mkdir, symlink and link
 */
static int create_new_entry(struct fuse_conn *fc, struct fuse_req *req,
			    struct inode *dir, struct dentry *entry,
			    int mode)
{
	struct fuse_entry_out outarg;
	struct inode *inode;
	int err;
	struct fuse_req *forget_req;

	forget_req = fuse_get_req(fc);
	if (IS_ERR(forget_req)) {
		fuse_put_request(fc, req);
		return PTR_ERR(forget_req);
	}

	req->in.h.nodeid = get_node_id(dir);
	req->out.numargs = 1;
	req->out.args[0].size = sizeof(outarg);
	req->out.args[0].value = &outarg;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (err)
		goto out_put_forget_req;

	err = -EIO;
	if (invalid_nodeid(outarg.nodeid))
		goto out_put_forget_req;

	if ((outarg.attr.mode ^ mode) & S_IFMT)
		goto out_put_forget_req;

	inode = fuse_iget(dir->i_sb, outarg.nodeid, outarg.generation,
			  &outarg.attr);
	if (!inode) {
		fuse_send_forget(fc, forget_req, outarg.nodeid, 1);
		return -ENOMEM;
	}
	fuse_put_request(fc, forget_req);

	if (S_ISDIR(inode->i_mode)) {
		struct dentry *alias;
		mutex_lock(&fc->inst_mutex);
		alias = d_find_alias(inode);
		if (alias) {
			/* New directory must have moved since mkdir */
			mutex_unlock(&fc->inst_mutex);
			dput(alias);
			iput(inode);
			return -EBUSY;
		}
		d_instantiate(entry, inode);
		mutex_unlock(&fc->inst_mutex);
	} else
		d_instantiate(entry, inode);

	fuse_change_timeout(entry, &outarg);
	fuse_invalidate_attr(dir);
	return 0;

 out_put_forget_req:
	fuse_put_request(fc, forget_req);
	return err;
}

static int fuse_mknod(struct inode *dir, struct dentry *entry, int mode,
		      dev_t rdev)
{
	struct fuse_mknod_in inarg;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.mode = mode;
	inarg.rdev = new_encode_dev(rdev);
	req->in.h.opcode = FUSE_MKNOD;
	req->in.numargs = 2;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = entry->d_name.len + 1;
	req->in.args[1].value = entry->d_name.name;
	return create_new_entry(fc, req, dir, entry, mode);
}

static int fuse_create(struct inode *dir, struct dentry *entry, int mode,
		       struct nameidata *nd)
{
#ifdef HAVE_LOOKUP_INSTANTIATE_FILP
	if (nd && (nd->flags & LOOKUP_CREATE)) {
		int err = fuse_create_open(dir, entry, mode, nd);
		if (err != -ENOSYS)
			return err;
		/* Fall back on mknod */
	}
#endif
	return fuse_mknod(dir, entry, mode, 0);
}

static int fuse_mkdir(struct inode *dir, struct dentry *entry, int mode)
{
	struct fuse_mkdir_in inarg;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.mode = mode;
	req->in.h.opcode = FUSE_MKDIR;
	req->in.numargs = 2;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = entry->d_name.len + 1;
	req->in.args[1].value = entry->d_name.name;
	return create_new_entry(fc, req, dir, entry, S_IFDIR);
}

static int fuse_symlink(struct inode *dir, struct dentry *entry,
			const char *link)
{
	struct fuse_conn *fc = get_fuse_conn(dir);
	unsigned len = strlen(link) + 1;
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->in.h.opcode = FUSE_SYMLINK;
	req->in.numargs = 2;
	req->in.args[0].size = entry->d_name.len + 1;
	req->in.args[0].value = entry->d_name.name;
	req->in.args[1].size = len;
	req->in.args[1].value = link;
	return create_new_entry(fc, req, dir, entry, S_IFLNK);
}

static int fuse_unlink(struct inode *dir, struct dentry *entry)
{
	int err;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->in.h.opcode = FUSE_UNLINK;
	req->in.h.nodeid = get_node_id(dir);
	req->in.numargs = 1;
	req->in.args[0].size = entry->d_name.len + 1;
	req->in.args[0].value = entry->d_name.name;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err) {
		struct inode *inode = entry->d_inode;

		/* Set nlink to zero so the inode can be cleared, if
                   the inode does have more links this will be
                   discovered at the next lookup/getattr */
		clear_nlink(inode);
		fuse_invalidate_attr(inode);
		fuse_invalidate_attr(dir);
		fuse_invalidate_entry_cache(entry);
	} else if (err == -EINTR)
		fuse_invalidate_entry(entry);
	return err;
}

static int fuse_rmdir(struct inode *dir, struct dentry *entry)
{
	int err;
	struct fuse_conn *fc = get_fuse_conn(dir);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->in.h.opcode = FUSE_RMDIR;
	req->in.h.nodeid = get_node_id(dir);
	req->in.numargs = 1;
	req->in.args[0].size = entry->d_name.len + 1;
	req->in.args[0].value = entry->d_name.name;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err) {
		clear_nlink(entry->d_inode);
		fuse_invalidate_attr(dir);
		fuse_invalidate_entry_cache(entry);
	} else if (err == -EINTR)
		fuse_invalidate_entry(entry);
	return err;
}

static int fuse_rename(struct inode *olddir, struct dentry *oldent,
		       struct inode *newdir, struct dentry *newent)
{
	int err;
	struct fuse_rename_in inarg;
	struct fuse_conn *fc = get_fuse_conn(olddir);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.newdir = get_node_id(newdir);
	req->in.h.opcode = FUSE_RENAME;
	req->in.h.nodeid = get_node_id(olddir);
	req->in.numargs = 3;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = oldent->d_name.len + 1;
	req->in.args[1].value = oldent->d_name.name;
	req->in.args[2].size = newent->d_name.len + 1;
	req->in.args[2].value = newent->d_name.name;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err) {
		fuse_invalidate_attr(olddir);
		if (olddir != newdir)
			fuse_invalidate_attr(newdir);

		/* newent will end up negative */
		if (newent->d_inode)
			fuse_invalidate_entry_cache(newent);
	} else if (err == -EINTR) {
		/* If request was interrupted, DEITY only knows if the
		   rename actually took place.  If the invalidation
		   fails (e.g. some process has CWD under the renamed
		   directory), then there can be inconsistency between
		   the dcache and the real filesystem.  Tough luck. */
		fuse_invalidate_entry(oldent);
		if (newent->d_inode)
			fuse_invalidate_entry(newent);
	}

	return err;
}

static int fuse_link(struct dentry *entry, struct inode *newdir,
		     struct dentry *newent)
{
	int err;
	struct fuse_link_in inarg;
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.oldnodeid = get_node_id(inode);
	req->in.h.opcode = FUSE_LINK;
	req->in.numargs = 2;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = newent->d_name.len + 1;
	req->in.args[1].value = newent->d_name.name;
	err = create_new_entry(fc, req, newdir, newent, inode->i_mode);
	/* Contrary to "normal" filesystems it can happen that link
	   makes two "logical" inodes point to the same "physical"
	   inode.  We invalidate the attributes of the old one, so it
	   will reflect changes in the backing inode (link count,
	   etc.)
	*/
	if (!err || err == -EINTR)
		fuse_invalidate_attr(inode);
	return err;
}

int fuse_do_getattr(struct inode *inode)
{
	int err;
	struct fuse_attr_out arg;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->in.h.opcode = FUSE_GETATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->out.numargs = 1;
	req->out.args[0].size = sizeof(arg);
	req->out.args[0].value = &arg;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err) {
		if ((inode->i_mode ^ arg.attr.mode) & S_IFMT) {
#ifndef KERNEL_2_6_12_PLUS
			if (get_node_id(inode) != FUSE_ROOT_ID)
				make_bad_inode(inode);
#else
			make_bad_inode(inode);
#endif
			err = -EIO;
		} else {
			struct fuse_inode *fi = get_fuse_inode(inode);
			fuse_change_attributes(inode, &arg.attr);
			fi->i_time = time_to_jiffies(arg.attr_valid,
						     arg.attr_valid_nsec);
		}
	}
	return err;
}

/*
 * Calling into a user-controlled filesystem gives the filesystem
 * daemon ptrace-like capabilities over the requester process.  This
 * means, that the filesystem daemon is able to record the exact
 * filesystem operations performed, and can also control the behavior
 * of the requester process in otherwise impossible ways.  For example
 * it can delay the operation for arbitrary length of time allowing
 * DoS against the requester.
 *
 * For this reason only those processes can call into the filesystem,
 * for which the owner of the mount has ptrace privilege.  This
 * excludes processes started by other users, suid or sgid processes.
 */
static int fuse_allow_task(struct fuse_conn *fc, struct task_struct *task)
{
	if (fc->flags & FUSE_ALLOW_OTHER)
		return 1;

	if (task->euid == fc->user_id &&
	    task->suid == fc->user_id &&
	    task->uid == fc->user_id &&
	    task->egid == fc->group_id &&
	    task->sgid == fc->group_id &&
	    task->gid == fc->group_id)
		return 1;

	return 0;
}

/*
 * Check whether the inode attributes are still valid
 *
 * If the attribute validity timeout has expired, then fetch the fresh
 * attributes with a 'getattr' request
 *
 * I'm not sure why cached attributes are never returned for the root
 * inode, this is probably being too cautious.
 */
static int fuse_revalidate(struct dentry *entry)
{
	struct inode *inode = entry->d_inode;
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_conn *fc = get_fuse_conn(inode);

	if (!fuse_allow_task(fc, current))
		return -EACCES;
	if (fi->i_time == (u64)-1) {
		/* The dentry was in use when the inode purge request was
		   processed. */
		shrink_dcache_parent(entry);
		d_drop(entry);
		return 0;
	}
	if (get_node_id(inode) != FUSE_ROOT_ID &&
	    fi->i_time >= get_jiffies_64())
		return 0;

	return fuse_do_getattr(inode);
}

static int fuse_access(struct inode *inode, int mask)
{
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;
	struct fuse_access_in inarg;
	int err;

	if (fc->no_access)
		return 0;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.mask = mask;
	req->in.h.opcode = FUSE_ACCESS;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 1;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (err == -ENOSYS) {
		fc->no_access = 1;
		err = 0;
	}
	return err;
}

/*
 * Check permission.  The two basic access models of FUSE are:
 *
 * 1) Local access checking ('default_permissions' mount option) based
 * on file mode.  This is the plain old disk filesystem permission
 * modell.
 *
 * 2) "Remote" access checking, where server is responsible for
 * checking permission in each inode operation.  An exception to this
 * is if ->permission() was invoked from sys_access() in which case an
 * access request is sent.  Execute permission is still checked
 * locally based on file mode.
 */
static int fuse_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	struct fuse_conn *fc = get_fuse_conn(inode);

	if (!fuse_allow_task(fc, current))
		return -EACCES;
	else if (fc->flags & FUSE_DEFAULT_PERMISSIONS) {
#ifdef KERNEL_2_6_10_PLUS
		int err = generic_permission(inode, mask, NULL);
#else
		int err = vfs_permission(inode, mask);
#endif

		/* If permission is denied, try to refresh file
		   attributes.  This is also needed, because the root
		   node will at first have no permissions */
		if (err == -EACCES) {
		 	err = fuse_do_getattr(inode);
			if (!err)
#ifdef KERNEL_2_6_10_PLUS
				err = generic_permission(inode, mask, NULL);
#else
				err = vfs_permission(inode, mask);
#endif
		}

		/* Note: the opposite of the above test does not
		   exist.  So if permissions are revoked this won't be
		   noticed immediately, only after the attribute
		   timeout has expired */

		return err;
	} else {
		int mode = inode->i_mode;
#ifndef KERNEL_2_6_11_PLUS
		if ((mask & MAY_WRITE) && IS_RDONLY(inode) &&
                    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
                        return -EROFS;
#endif
		if ((mask & MAY_EXEC) && !S_ISDIR(mode) && !(mode & S_IXUGO))
			return -EACCES;

#ifndef LOOKUP_CHDIR
#define LOOKUP_CHDIR 0
#endif
		if (nd && (nd->flags & (LOOKUP_ACCESS | LOOKUP_CHDIR)))
			return fuse_access(inode, mask);
		return 0;
	}
}

static int parse_dirfile(char *buf, size_t nbytes, struct file *file,
			 void *dstbuf, filldir_t filldir)
{
	while (nbytes >= FUSE_NAME_OFFSET) {
		struct fuse_dirent *dirent = (struct fuse_dirent *) buf;
		size_t reclen = FUSE_DIRENT_SIZE(dirent);
		int over;
		if (!dirent->namelen || dirent->namelen > FUSE_NAME_MAX)
			return -EIO;
		if (reclen > nbytes)
			break;

		over = filldir(dstbuf, dirent->name, dirent->namelen,
			       file->f_pos, dirent->ino, dirent->type);
		if (over)
			break;

		buf += reclen;
		nbytes -= reclen;
		file->f_pos = dirent->off;
	}

	return 0;
}

static int fuse_readdir(struct file *file, void *dstbuf, filldir_t filldir)
{
	int err;
	size_t nbytes;
	struct page *page;
	struct inode *inode = file->f_dentry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;

	if (is_bad_inode(inode))
		return -EIO;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		fuse_put_request(fc, req);
		return -ENOMEM;
	}
	req->num_pages = 1;
	req->pages[0] = page;
	fuse_read_fill(req, file, inode, file->f_pos, PAGE_SIZE, FUSE_READDIR);
	request_send(fc, req);
	nbytes = req->out.args[0].size;
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err)
		err = parse_dirfile(page_address(page), nbytes, file, dstbuf,
				    filldir);

	__free_page(page);
	fuse_invalidate_attr(inode); /* atime changed */
	return err;
}

static char *read_link(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req = fuse_get_req(fc);
	char *link;

	if (IS_ERR(req))
		return ERR_PTR(PTR_ERR(req));

	link = (char *) __get_free_page(GFP_KERNEL);
	if (!link) {
		link = ERR_PTR(-ENOMEM);
		goto out;
	}
	req->in.h.opcode = FUSE_READLINK;
	req->in.h.nodeid = get_node_id(inode);
	req->out.argvar = 1;
	req->out.numargs = 1;
	req->out.args[0].size = PAGE_SIZE - 1;
	req->out.args[0].value = link;
	request_send(fc, req);
	if (req->out.h.error) {
		free_page((unsigned long) link);
		link = ERR_PTR(req->out.h.error);
	} else
		link[req->out.args[0].size] = '\0';
 out:
	fuse_put_request(fc, req);
	fuse_invalidate_attr(inode); /* atime changed */
	return link;
}

static void free_link(char *link)
{
	if (!IS_ERR(link))
		free_page((unsigned long) link);
}

#ifdef KERNEL_2_6_13_PLUS
static void *fuse_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	nd_set_link(nd, read_link(dentry));
	return NULL;
}

static void fuse_put_link(struct dentry *dentry, struct nameidata *nd, void *c)
{
	free_link(nd_get_link(nd));
}
#else
static int fuse_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	nd_set_link(nd, read_link(dentry));
	return 0;
}

static void fuse_put_link(struct dentry *dentry, struct nameidata *nd)
{
	free_link(nd_get_link(nd));
}
#endif

static int fuse_dir_open(struct inode *inode, struct file *file)
{
	return fuse_open_common(inode, file, 1);
}

static int fuse_dir_release(struct inode *inode, struct file *file)
{
	return fuse_release_common(inode, file, 1);
}

static int fuse_dir_fsync(struct file *file, struct dentry *de, int datasync)
{
	/* nfsd can call this with no file */
	return file ? fuse_fsync_common(file, de, datasync, 1) : 0;
}

static void iattr_to_fattr(struct iattr *iattr, struct fuse_setattr_in *arg)
{
	unsigned ivalid = iattr->ia_valid;

	if (ivalid & ATTR_MODE)
		arg->valid |= FATTR_MODE,   arg->mode = iattr->ia_mode;
	if (ivalid & ATTR_UID)
		arg->valid |= FATTR_UID,    arg->uid = iattr->ia_uid;
	if (ivalid & ATTR_GID)
		arg->valid |= FATTR_GID,    arg->gid = iattr->ia_gid;
	if (ivalid & ATTR_SIZE)
		arg->valid |= FATTR_SIZE,   arg->size = iattr->ia_size;
	/* You can only _set_ these together (they may change by themselves) */
	if ((ivalid & (ATTR_ATIME | ATTR_MTIME)) == (ATTR_ATIME | ATTR_MTIME)) {
		arg->valid |= FATTR_ATIME | FATTR_MTIME;
		arg->atime = iattr->ia_atime.tv_sec;
		arg->mtime = iattr->ia_mtime.tv_sec;
	}
#ifdef ATTR_FILE
	if (ivalid & ATTR_FILE) {
		struct fuse_file *ff = iattr->ia_file->private_data;
		arg->valid |= FATTR_FH;
		arg->fh = ff->fh;
	}
#endif
}

static void fuse_vmtruncate(struct inode *inode, loff_t offset)
{
	struct fuse_conn *fc = get_fuse_conn(inode);
	int need_trunc;

	spin_lock(&fc->lock);
	need_trunc = inode->i_size > offset;
	i_size_write(inode, offset);
	spin_unlock(&fc->lock);

	if (need_trunc) {
		struct address_space *mapping = inode->i_mapping;
		unmap_mapping_range(mapping, offset + PAGE_SIZE - 1, 0, 1);
		truncate_inode_pages(mapping, offset);
	}
}

/*
 * Set attributes, and at the same time refresh them.
 *
 * Truncation is slightly complicated, because the 'truncate' request
 * may fail, in which case we don't want to touch the mapping.
 * vmtruncate() doesn't allow for this case, so do the rlimit checking
 * and the actual truncation by hand.
 */
static int fuse_setattr(struct dentry *entry, struct iattr *attr)
{
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_req *req;
	struct fuse_setattr_in inarg;
	struct fuse_attr_out outarg;
	int err;
	int is_truncate = 0;

	if (fc->flags & FUSE_DEFAULT_PERMISSIONS) {
		err = inode_change_ok(inode, attr);
		if (err)
			return err;
	}

	if (attr->ia_valid & ATTR_SIZE) {
		unsigned long limit;
		is_truncate = 1;
		if (IS_SWAPFILE(inode))
			return -ETXTBSY;
#ifdef KERNEL_2_6_10_PLUS
		limit = current->signal->rlim[RLIMIT_FSIZE].rlim_cur;
#else
		limit = current->rlim[RLIMIT_FSIZE].rlim_cur;
#endif
		if (limit != RLIM_INFINITY && attr->ia_size > (loff_t) limit) {
			send_sig(SIGXFSZ, current, 0);
			return -EFBIG;
		}
	}

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	iattr_to_fattr(attr, &inarg);
	/* Defend against future expansion of ATTR_FILE use */
	if (S_ISDIR(inode->i_mode))
		inarg.valid &= ~FATTR_FH;
	req->in.h.opcode = FUSE_SETATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 1;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->out.numargs = 1;
	req->out.args[0].size = sizeof(outarg);
	req->out.args[0].value = &outarg;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (!err) {
		if ((inode->i_mode ^ outarg.attr.mode) & S_IFMT) {
#ifndef KERNEL_2_6_12_PLUS
			if (get_node_id(inode) != FUSE_ROOT_ID)
				make_bad_inode(inode);
#else
			make_bad_inode(inode);
#endif
			err = -EIO;
		} else {
			if (is_truncate)
				fuse_vmtruncate(inode, outarg.attr.size);
			fuse_change_attributes(inode, &outarg.attr);
			fi->i_time = time_to_jiffies(outarg.attr_valid,
						     outarg.attr_valid_nsec);
		}
	} else if (err == -EINTR)
		fuse_invalidate_attr(inode);

	return err;
}

static int fuse_getattr(struct vfsmount *mnt, struct dentry *entry,
			struct kstat *stat)
{
	struct inode *inode = entry->d_inode;
	int err = fuse_revalidate(entry);
	if (!err)
		/* FIXME: may want specialized function because of
		   st_blksize on block devices on 2.6.19+ */
		generic_fillattr(inode, stat);

	return err;
}

static int fuse_setxattr(struct dentry *entry, const char *name,
			 const void *value, size_t size, int flags)
{
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;
	struct fuse_setxattr_in inarg;
	int err;

	if (fc->no_setxattr)
		return -EOPNOTSUPP;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.size = size;
	inarg.flags = flags;
	req->in.h.opcode = FUSE_SETXATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 3;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = strlen(name) + 1;
	req->in.args[1].value = name;
	req->in.args[2].size = size;
	req->in.args[2].value = value;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (err == -ENOSYS) {
		fc->no_setxattr = 1;
		err = -EOPNOTSUPP;
	}
	return err;
}

static ssize_t fuse_getxattr(struct dentry *entry, const char *name,
			     void *value, size_t size)
{
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;
	struct fuse_getxattr_in inarg;
	struct fuse_getxattr_out outarg;
	ssize_t ret;

	if (fc->no_getxattr)
		return -EOPNOTSUPP;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.size = size;
	req->in.h.opcode = FUSE_GETXATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 2;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	req->in.args[1].size = strlen(name) + 1;
	req->in.args[1].value = name;
	/* This is really two different operations rolled into one */
	req->out.numargs = 1;
	if (size) {
		req->out.argvar = 1;
		req->out.args[0].size = size;
		req->out.args[0].value = value;
	} else {
		req->out.args[0].size = sizeof(outarg);
		req->out.args[0].value = &outarg;
	}
	request_send(fc, req);
	ret = req->out.h.error;
	if (!ret)
		ret = size ? req->out.args[0].size : outarg.size;
	else {
		if (ret == -ENOSYS) {
			fc->no_getxattr = 1;
			ret = -EOPNOTSUPP;
		}
	}
	fuse_put_request(fc, req);
	return ret;
}

static ssize_t fuse_listxattr(struct dentry *entry, char *list, size_t size)
{
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;
	struct fuse_getxattr_in inarg;
	struct fuse_getxattr_out outarg;
	ssize_t ret;

	if (fc->no_listxattr)
		return -EOPNOTSUPP;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	memset(&inarg, 0, sizeof(inarg));
	inarg.size = size;
	req->in.h.opcode = FUSE_LISTXATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 1;
	req->in.args[0].size = sizeof(inarg);
	req->in.args[0].value = &inarg;
	/* This is really two different operations rolled into one */
	req->out.numargs = 1;
	if (size) {
		req->out.argvar = 1;
		req->out.args[0].size = size;
		req->out.args[0].value = list;
	} else {
		req->out.args[0].size = sizeof(outarg);
		req->out.args[0].value = &outarg;
	}
	request_send(fc, req);
	ret = req->out.h.error;
	if (!ret)
		ret = size ? req->out.args[0].size : outarg.size;
	else {
		if (ret == -ENOSYS) {
			fc->no_listxattr = 1;
			ret = -EOPNOTSUPP;
		}
	}
	fuse_put_request(fc, req);
	return ret;
}

static int fuse_removexattr(struct dentry *entry, const char *name)
{
	struct inode *inode = entry->d_inode;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_req *req;
	int err;

	if (fc->no_removexattr)
		return -EOPNOTSUPP;

	req = fuse_get_req(fc);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->in.h.opcode = FUSE_REMOVEXATTR;
	req->in.h.nodeid = get_node_id(inode);
	req->in.numargs = 1;
	req->in.args[0].size = strlen(name) + 1;
	req->in.args[0].value = name;
	request_send(fc, req);
	err = req->out.h.error;
	fuse_put_request(fc, req);
	if (err == -ENOSYS) {
		fc->no_removexattr = 1;
		err = -EOPNOTSUPP;
	}
	return err;
}

static struct inode_operations fuse_dir_inode_operations = {
	.lookup		= fuse_lookup,
	.mkdir		= fuse_mkdir,
	.symlink	= fuse_symlink,
	.unlink		= fuse_unlink,
	.rmdir		= fuse_rmdir,
	.rename		= fuse_rename,
	.link		= fuse_link,
	.setattr	= fuse_setattr,
	.create		= fuse_create,
	.mknod		= fuse_mknod,
	.permission	= fuse_permission,
	.getattr	= fuse_getattr,
	.setxattr	= fuse_setxattr,
	.getxattr	= fuse_getxattr,
	.listxattr	= fuse_listxattr,
	.removexattr	= fuse_removexattr,
};

static struct file_operations fuse_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= fuse_readdir,
	.open		= fuse_dir_open,
	.release	= fuse_dir_release,
	.fsync		= fuse_dir_fsync,
};

static struct inode_operations fuse_common_inode_operations = {
	.setattr	= fuse_setattr,
	.permission	= fuse_permission,
	.getattr	= fuse_getattr,
	.setxattr	= fuse_setxattr,
	.getxattr	= fuse_getxattr,
	.listxattr	= fuse_listxattr,
	.removexattr	= fuse_removexattr,
};

static struct inode_operations fuse_symlink_inode_operations = {
	.setattr	= fuse_setattr,
	.follow_link	= fuse_follow_link,
	.put_link	= fuse_put_link,
	.readlink	= generic_readlink,
	.getattr	= fuse_getattr,
	.setxattr	= fuse_setxattr,
	.getxattr	= fuse_getxattr,
	.listxattr	= fuse_listxattr,
	.removexattr	= fuse_removexattr,
};

void fuse_init_common(struct inode *inode)
{
	inode->i_op = &fuse_common_inode_operations;
}

void fuse_init_dir(struct inode *inode)
{
	inode->i_op = &fuse_dir_inode_operations;
	inode->i_fop = &fuse_dir_operations;
}

void fuse_init_symlink(struct inode *inode)
{
	inode->i_op = &fuse_symlink_inode_operations;
}
