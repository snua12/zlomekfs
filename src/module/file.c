/*! \file
    \brief File and address space operations. */

/*
   Copyright (C) 2004 Martin Zlomek
   Copyright (C) 2004 Josef Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/pagemap.h>
#include <linux/string.h>

#include "zfs.h"
#include "zfs-prot.h"
#include "zfsd-call.h"

//#define ZFS_READWRITE_OLD

#ifdef ZFS_READWRITE_OLD
static ssize_t zfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
        struct inode *inode = file->f_dentry->d_inode;
        read_args args;
        int error;
        size_t total = 0;
        data_buffer data;

        TRACE("reading file'%s' from %lld size %u", file->f_dentry->d_name.name, *off, nbytes);
        TRACE("inode size is: %lld", i_size_read(inode));

        data.user = true;
        data.buf.u_wbuf = buf;
        data.len = nbytes;

        args.cap = *CAP(file->private_data);
        while (nbytes > 0) {
                args.offset = *off;
                args.count = (nbytes > ZFS_MAXDATA) ? ZFS_MAXDATA : nbytes;

                error = zfsd_read(&data, &args);
                if (error >= 0) {
                        *off += error;
                        data.buf.u_wbuf += error;
                        total += error;
                        nbytes -= error;
                        inode->i_atime = CURRENT_TIME;
                        if (*off > i_size_read(inode)) {
                                i_size_write(inode, *off);
                                inode->i_ctime = CURRENT_TIME;
                        }
                        if (error < args.count)
                                break;
                } else {
                        if (error == -ESTALE)
                                ZFS_I(inode)->flags |= NEED_REVALIDATE;
                        return error;
                }
        }

        TRACE("total: %u", total);

        return total;
}

static ssize_t zfs_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
        struct inode *inode = file->f_dentry->d_inode;
        write_args args;
        int error;
        int total = 0;

        TRACE("'%s': %lld", file->f_dentry->d_name.name, *off);

        args.cap = *CAP(file->private_data);
        while (nbytes > 0) {
                args.offset = (file->f_flags & O_APPEND) ? i_size_read(inode) : *off;
                args.data.user = true;
                args.data.len = (nbytes > ZFS_MAXDATA) ? ZFS_MAXDATA : nbytes;
                args.data.buf.u_rbuf = buf;

                error = zfsd_write(&args);
                if (error >= 0) {
                        *off = args.offset + error;
                        buf += error;
                        total += error;
                        nbytes -= error;
                        inode->i_mtime = CURRENT_TIME;
                        if (*off > i_size_read(inode)) {
                                i_size_write(inode, *off);
                                inode->i_ctime = CURRENT_TIME;
                        }
                        if (error < args.data.len)
                                break;
                } else {
                        if (error == -ESTALE)
                                ZFS_I(inode)->flags |= NEED_REVALIDATE;
                        return error;
                }
        }

        return total;
}
#else

/*! \brief Read part of file into userspace buffer.
 *
 * Handled by VFS's do_sync_read() which uses the kernel page cache. The actual reads are handled by #zfs_writepage().
 */
static ssize_t zfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *off)
{
        struct inode *inode = file->f_dentry->d_inode;

        TRACE("reading file '%s' from %lld size %u", file->f_dentry->d_name.name, *off, nbytes);
        TRACE("inode size is: %lld", i_size_read(inode));
        TRACE("calling do_sync_read()");

        ssize_t res = do_sync_read(file, buf, nbytes, off);

        TRACE("do_sync_read() result: %u", res);

        return res;
}

/*! \brief Write out part of file from userspace buffer.
 *
 * Handled by VFS's do_sync_read() which uses kernel page cache, VFS's buffered_write(),
 * which calls #zfs_prepare_write() and #zfs_commit_write() for synced write through page cache.
 */
static ssize_t zfs_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *off)
{
        struct inode *inode = file->f_dentry->d_inode;

        TRACE("writing file '%s' from %lld size %u", file->f_dentry->d_name.name, *off, nbytes);
        TRACE("inode size is: %lld", i_size_read(inode));
        TRACE("calling do_sync_write()");

        ssize_t res = do_sync_write(file, buf, nbytes, off);

        TRACE("do_sync_write() result: %u", res);

        return res;
}
#endif

/*! \brief Called when user opens a file.
 *
 * Acquires the capability from userspace daemon and stores it into file's private data and ZFS extended inode.
 *
 * \retval 0 when OK, negative otherwise
 */
int zfs_open(struct inode *inode, struct file *file)
{
        struct dentry *dentry = file->f_dentry;
        zfs_cap *cap = NULL;
        open_args args;
        int error;

        TRACE("'%s'", dentry->d_name.name);

        if ((file->f_flags & O_CREAT) && dentry->d_fsdata) {
                /* We already have CAP for the file (zfs_create() has found it out). */
                file->private_data = dentry->d_fsdata;
                ZFS_I(inode)->cap = dentry->d_fsdata;
                dentry->d_fsdata = NULL;
        } else {
                /* Create new capability */
                cap = kmalloc(sizeof(zfs_cap) + (S_ISDIR(inode->i_mode) ? sizeof(int32_t) : 0), GFP_KERNEL);
                if (!cap)
                        return -ENOMEM;

                args.file = ZFS_I(inode)->fh;
                args.flags = file->f_flags;

                /* fill the capability by user daemon */
                error = zfsd_open(cap, &args);
                if (error) {
                        kfree(cap);
                        if (error == -ESTALE)
                                ZFS_I(inode)->flags |= NEED_REVALIDATE;
                        return error;
                }

                file->private_data = cap;
                ZFS_I(inode)->cap = cap;
        }

        return 0;
}

/*! \brief Called when user closes a file
 *
 * Writes out dirty pages and then invalidates are valid pages, for the session synchronization with zfsd.
 *
 * \retval 0 when OK, negative otherwise
 */
int zfs_release(struct inode *inode, struct file *file)
{
        int error;

        TRACE("name: '%s', inode: %p, file->dentry->inode: %p", file->f_dentry->d_name.name, inode, file->f_dentry->d_inode);

        /* writes back dirty pages if the file closed is regular */
        if (S_ISREG(inode->i_mode)) {

                TRACE("writing inode");

                error = write_inode_now(inode, 1);

                TRACE("writing inode result: %d", error);
        }

        /* return capability to zfsd */
        error = zfsd_close(CAP(file->private_data));


        /* invalidates all pages if the file closed is regular */
        if (S_ISREG(inode->i_mode)) {

                TRACE("invalidating pages");

                unsigned long inv = invalidate_inode_pages(inode->i_mapping);

                TRACE("invalidated %lu pages", inv);
        }

        /* remove the capability from inode and file and free the memory */
        ZFS_I(inode)->cap = NULL;
        kfree(file->private_data);

        return error;
}

/*! \brief Reads the specified part of page from opened file.
 *
 * Reads maximum of LENGTH bytes from PAGE from FILE via zfsd call.
 * The rest of page is zeroed and page is marked as UpToDate.
 *
 * \retval 0 when OK, negative otherwise
 */
static int _zfs_readpage(struct file *file, struct page *page, unsigned length)
{
        read_args args;
        int error = 0;
        data_buffer data;

        TRACE("'%s': page %lu, length %u", file->f_dentry->d_name.name, page->index, length);

        /* Page is already updated, no need to read */
        if (PageUptodate(page))
                goto out;

    /* prepare the zfsd call arguments */
        args.cap = *CAP(file->private_data);
        args.offset = page->index << PAGE_CACHE_SHIFT;
        args.count = length;

    /* map the page into virtual adress writable from kernel */
        data.user = false;
        data.buf.k_buf = kmap(page);
        data.len = length;

    /* read the data from zfsd */
        error = zfsd_read(&data, &args);
        if (error >= 0) {
                if (error < PAGE_CACHE_SIZE)
                        /* Zero the rest of the page. */
                        memset(data.buf.k_buf + error, 0, PAGE_CACHE_SIZE - error);

                SetPageUptodate(page);

                error = 0;
        } else if (error == -ESTALE)
                ZFS_I(file->f_dentry->d_inode)->flags |= NEED_REVALIDATE;

        kunmap(page);

out:
        return error;
}

/*! \brief Reads the whole page from opened file.
 *
 * Called by VFS when accessing mmapped memory which is not uptodate, and when reading from file.
 * Uses #_zfs_readpage() to read the whole page from zfsd.
 * VFS expects it to unlock the page when done.
 *
 * \retval 0 when OK, negative otherwise
 */
static int zfs_readpage(struct file *file, struct page *page)
{
        int error = _zfs_readpage(file, page, PAGE_CACHE_SIZE);

        unlock_page(page);

        return error;
}

/*! \brief Write the part of file into zfsd using capability CAP.
 *
 * Used by #zfs_commit_write() and #zfs_writepage() to write specified interval from kernel page cache into zfsd.
 *
 * \retval Number of bytes actually written out.
 */
static unsigned _zfs_write_cap(zfs_cap * cap, char * kaddr, uint64_t from, unsigned length)
{
        write_args args;

        int error = 0;

        TRACE("writing capability from %llu, length %u", from, length);

        args.cap = *cap;
        args.data.user = false;
        args.offset = from;
        args.data.buf.k_buf = kaddr;

        /* write until we have something or error occurs */
        while (length > 0) {
                args.data.len = (length > ZFS_MAXDATA) ? ZFS_MAXDATA : length;

                error = zfsd_write(&args);

                if (error > 0) {
                        args.offset += error;
                        args.data.buf.k_buf += error;
                        length -= error;
                } else {
                        TRACE("return %d", error);
                        return error;
                }
        }

        TRACE("written total of %u bytes", (unsigned)(args.offset - from));
        return args.offset - from;
}


/*! \brief Writes the data in page out into zfsd.
 *
 * Called by VFS for dirty pages when performing msync() or munmap(), and by #zfs_close()
 * via VFS function write_inode_now().
 *
 * \retval 0 when OK (not number of bytes written!), negative otherwise
 */
static int zfs_writepage(struct page *page, struct writeback_control *wbc)
{
        /* get the inode from page mapping */
        struct inode *inode = page->mapping->host;

        char * kaddr;
        /* lenght of data to write, usually whole page */
        unsigned length = PAGE_CACHE_SIZE;

        /* return value */
        int error = 0;
        /* get current size of the file in bytes */
        loff_t i_size = i_size_read(inode);
        /* get number of the last page */
        unsigned long end_index = i_size >> PAGE_CACHE_SHIFT;

        TRACE("writing page %lu", page->index);

        /* easy case, whole page will be written out */
        if (page->index < end_index) {
                TRACE("writing whole page %lu", page->index);
                goto do_it;
        }

        /* now it can be only part of the last page of file or nothing */
        length = i_size % PAGE_CACHE_SIZE;

        if (page->index > end_index || !length) {
                TRACE("nothing to write with page %lu", page->index);
                goto out;
        }

do_it:

        /* we already returned the capability, file was closed, we can't write no more
         * could happen maybe only if user mmaped something and then closed the file and then dirtied the mapped area */
        if (ZFS_I(inode)->cap == NULL) {
                TRACE("WARNING: CAP == NULL\n");
                ZFS_I(inode)->flags |= NEED_REVALIDATE;
                error = -ESTALE;
                goto out;
        }

        kaddr = kmap(page);

        error = _zfs_write_cap(ZFS_I(inode)->cap, kaddr, (page->index << PAGE_CACHE_SHIFT), length);

        kunmap(page);

    /* if no error occured, return 0 (writepage does _not_ return num of bytes written! */
        if (error >= 0) {
          inode->i_mtime = CURRENT_TIME;
          error = 0;
        } else {
          ClearPageUptodate(page);
          if (error == -ESTALE)
                ZFS_I(inode)->flags |= NEED_REVALIDATE;
        }

out:

        unlock_page(page);
        TRACE("returning: %d", error);
        return error;
}

/*! \brief Called from VFS for pages affected by user's write()
 *
 * Called before copying the data from user space into the page. If the page is not UpToDate,
 * this functions reads/zeroes the parts of page not affected by the write, so after the write
 * we will have fully UpToDate page.
 *
 * \retval 0 when OK, negative otherwise
 */
static int zfs_prepare_write(struct file *file, struct page *page,
                        unsigned from, unsigned to)
{
        int err = 0;

        TRACE("'%s': %lu", file->f_dentry->d_name.name, page->index);

        /* If the page is UpToDate, nothing to do */
        if (!PageUptodate(page)) {
                struct inode *inode = page->mapping->host;
                /* position of the beginning of this page in the file */
                loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT);

                unsigned length = 0;

                /* reading has sense only if the file is bigger than this page's beginning */
                if (pos < inode->i_size) {
                        /* makes it read either whole page or just the part before from */
                        /* if whole page will be written, result is 0, therefore no reading */
                        length = (to < PAGE_CACHE_SIZE) ? PAGE_CACHE_SIZE : from;
                        /* trim the length if the file size is smaller */
                        length = ((unsigned)(inode->i_size - pos) < length) ? (unsigned)(inode->i_size - pos) : length;
                }

                /* if there's anything to read, read it, the _zfs_readpage() will zero the rest of page itself */
                if (length > 0) {
                        err = _zfs_readpage(file, page, length);
                } else {
                        /* if not, zero the whole page instead */
                        char * kaddr = kmap(page);
                        memset(kaddr, 0, PAGE_CACHE_SIZE);
                        kunmap(page);
                }

                if (!err) {
                        SetPageUptodate(page);
                }
        }

        return err;
}

/*! \brief Commit the user's write into file
 *
 * Called by VFS, after the page gets filled with data copied from user's buffer.
 * Performs writethrough of these data into zfsd. The dirty status doesn't need to get changed then
 *
 * \retval 0 when OK, negative otherwise
 */
static int zfs_commit_write(struct file *file, struct page *page,
                        unsigned offset, unsigned to)
{
        struct inode *inode = page->mapping->host;
        loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;
        int err = 0;
        char *kaddr;

        TRACE("'%s': page %lu from %u to %u", file->f_dentry->d_name.name, page->index, offset, to);

    /* change the inode's file size if the write gets pass the actual one */
        if (pos > inode->i_size)
                i_size_write(inode, pos);

        kaddr = kmap(page);

        /* write the data */
        err = _zfs_write_cap(CAP(file->private_data), kaddr + offset,
                                                 (page->index << PAGE_CACHE_SHIFT) + offset, to - offset);

        kunmap(kaddr);

        inode->i_mtime = CURRENT_TIME;

        return err;
}

/*! \brief Functions for struct file operations.
 */
struct file_operations zfs_file_operations = {
        .llseek         = generic_file_llseek, /*!< Generic VFS function for seeking */
        .read			= zfs_read,            /*!< See #zfs_read() */
        .write			= zfs_write,           /*!< See #zfs_write() */

#ifdef ZFS_READWRITE_OLD
//	.mmap			= generic_file_readonly_mmap,
#else

        .aio_read		= generic_file_aio_read,  /*!< Generic VFS function, uses the kernel page cache for readahead */
        .aio_write		= generic_file_aio_write, /*!< Generic VFS function, uses #zfs_prepare_write() and #zfs_commit_write() */
        .mmap			= generic_file_mmap,      /*!< Generic VFS function for read/write mmap() */

#endif

        .open           = zfs_open,			/*!< See #zfs_open() */
        .release        = zfs_release,      /*!< See #zfs_release() */
};

/*! \brief Functions for inode's addres space operations
 */
struct address_space_operations zfs_file_address_space_operations = {
        .readpage           = zfs_readpage,    /*!< See #zfs_readpage() */
        .writepage			= zfs_writepage,   /*!< See #zfs_writepage() */

#ifndef ZFS_READWRITE_OLD
        .prepare_write 		= zfs_prepare_write,  /*!< See #zfs_prepare_write() */
        .commit_write 		= zfs_commit_write,   /*!< See #zfs_commit_write() */
#endif
};

/** \page mmap Support for mmap() in zfs module.
 *
 * Formerly, the ZFS kernel module had just readonly mmap() support, using VFS's generic_file_readonly_mmap().
 * It was intended mainly for execution of binaries, and the data which were read into kernel page cache this way,
 * weren't synchronized with generic file reads and writes.
 *
 * Now the module has full read and write mmap() support, and all file reads and writes go through the kernel page cache.
 * The #zfs_read() function doesn't read the data from user space addres no more. It just calls VFS's do_sync_read() which
 * uses generic_file_aio_read(), which reads the data from kernel page cache and performs read-ahead caching. The actual
 * page reading (only for pages that are not uptodate) is done by #zfs_readpage(), which fetches the data from zfsd daemon.
 * Writes are handled by #zfs_write() which calls VFS's do_sync_write(), which assures all data is written through before
 * the user program continues. VFS's function calculate, which parts of which pages are affected by the write and call first
 * the #zfs_prepare_write(). This function checks if the page is uptodate, and eventually fetch the part of page not affected
 * by the write, from zfsd. This makes the page uptodate after the write has finished. The VFS then copies data from user
 * space and calls #zfs_commit_write(), which writes the affected part of page into zfsd. All pages affected by the write are
 * handled by these two functions this way. When some part of file is mmaped, the kernel calls #zfs_readpage() when updated
 * data are needed, and flushes dirty pages with #zfs_writepage() when it needs to free some pages, or user requests msync()
 * or munmap(). The #zfs_release() function, called when user closes a file, also flushes dirty pages via VFS function
 * write_inode_now() and then invalidates all inode's pages by VFS invalidate_inode_pages(). This way, the session semantics
 * is supported.
 */
