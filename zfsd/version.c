/*! \file
    \brief Functions for file versioning.  */

/* Copyright (C) 2010 Rastislav Wartiak

   This file is part of zlomekFS.

   zlomekFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   zlomekFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <libgen.h>
#include <syscall.h>
#ifdef __linux__
#ifdef HAVE_LINUX_DIRENT_H
#include <linux/dirent.h>
#endif /* HAVE_LINUX_DIRENT_H */
#include <linux/unistd.h>
#else
#include <dirent.h>
#endif
#include "fh.h"
#include "log.h"
#include "utime.h"
#include "dir.h"
#include "user-group.h"
#include "memory.h"
#include "metadata.h"
#include "version.h"
#include "update.h"

#ifdef VERSIONS

#ifdef __linux__

/* FIXME: VB: New systems don't have linux/dirent.h, man getdents says you
 * have to define it yourself. It would be really less fragile to use
 * readdir(3), but that would mean obtaining DIR * from fd through fdopendir()
 * which says you have to give your fd up, which I am not sure about.
 * Also it would need to use cookie for telldir/seekdir instead of lseek.
 */
#ifndef HAVE_LINUX_DIRENT_H

struct dirent {
    long           d_ino;
    __off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};

#endif /* HAVE_LINUX_DIRENT_H */

static int
getdents (int fd, struct dirent *dirp, unsigned count)
{
  return syscall (SYS_getdents, fd, dirp, count);
}

#endif /* __linux__ */

/*! Hash function for file name.  */
#define DIRHTAB_HASH(N) (crc32_string((N)->name))

/*! \brief Generate hash for dirtab item
 *
 * Generate hash for dirtab item.
 *
 *  \param x Item to hash
 *
 *  \retval hashed value
 *
 *  \see dirtab_item_def
*/
hash_t dirhtab_hash (const void *x)
{
  return DIRHTAB_HASH((const struct dirhtab_item_def *)x);
}

/*! \brief Compare two dirtab items
 *
 * Compare two dirtab items.
 *
 *  \param x First item
 *  \param y Second item
 *
 *  \see dirtab_item_def
*/
int dirhtab_eq (const void *x, const void *y)
{
  return (!strcmp (((const struct dirhtab_item_def *)x)->name, ((const struct dirhtab_item_def *)y)->name));
}

/*! \brief Delete item from the dirtab hash table
 *
 * Delete item from the hash table.
 *
 *  \param x Item to delete
 *
 *  \see dirtab_item_def
*/
void dirhtab_del (void *x)
{
  struct dirhtab_item_def *i = (struct dirhtab_item_def *)x;

  if (i->name)
    {
      free (i->name);
      i->name = NULL;
    }

  free (i);
}

/*! \brief Prepare hash table before readdir
 *
 * Prepare hash table before readdir.
 *
 *  \param dentry Dentry of the directory
*/
void
version_create_dirhtab (internal_dentry dentry)
{
  if (dentry->dirhtab)
    htab_destroy(dentry->dirhtab);

  dentry->dirhtab = htab_create(10, dirhtab_hash, dirhtab_eq, dirhtab_del, &dentry->fh->mutex);
}

/*! \brief Return versions during readdir
 *
 * Return version file names for the hash table during readdir. Hash table is first filled in by
 * version_readdir_fill_dirhtab.
 * All parameters are taken from readdir call.
 *
 *  \param list
 *  \param dentry
 *  \param cookie
 *  \param data
 *  \param filldir
 *
 *  \see version_readdir_from_dirhtab
*/
int32_t
version_readdir_from_dirhtab (dir_list *list, internal_dentry dentry, int32_t cookie,
    readdir_data *data, filldir_f filldir)
{
  unsigned int i;

  // retrieve from hash table
  for (i = 0; i < dentry->dirhtab->size; i++)
    {
      struct dirhtab_item_def *e = dentry->dirhtab->table[i];

      if ((e == EMPTY_ENTRY) || (e == DELETED_ENTRY))
        continue;

      if (!(*filldir) (e->ino, cookie, e->name, strlen (e->name), list, data))
        break;

      htab_clear_slot (dentry->dirhtab, &dentry->dirhtab->table[i]);
    }

  RETURN_INT (ZFS_OK);
}

/*! \brief Store version during readdir
 *
 * Store version file information into a hash table associated with the dentry. It is called during
 * readdir to find newest version for every versioned file.
 *
 *  \param dentry Directory dentry
 *  \param stamp Version time stamp
 *  \param ino Inode number of the version file
 *  \param name File name
*/
int32_t
version_readdir_fill_dirhtab (internal_dentry dentry, time_t stamp, long ino, char *name)
{
  struct dirhtab_item_def **x;
  struct dirhtab_item_def i;

  i.ino = ino;
  i.name = name; // only link for now, will copy it later if needed
  i.stamp = stamp;

  zfsd_mutex_lock (&dentry->fh->mutex);
  x = (struct dirhtab_item_def **) htab_find_slot (dentry->dirhtab, &i, INSERT);
  if (!x)
    {
      message (LOG_WARNING, FACILITY_VERSION,
          "Problem finding hash slot: name=%s, stamp=%ld\n", i.name, i.stamp);
    }
  else if (*x)
    {
      // file is already there
      if (stamp && (stamp < (*x)->stamp))
        {
          (*x)->ino = ino;
          (*x)->stamp = stamp;
        }
    }
  else
    {
      // duplicate string, it was linked temporarily
      struct dirhtab_item_def *n;
      n = xmalloc (sizeof (struct dirhtab_item_def));
      n->ino = ino;
      n->name = xstrdup (name);
      n->stamp = stamp;
      *x = n;
    }
  zfsd_mutex_unlock (&dentry->fh->mutex);

  RETURN_INT (ZFS_OK);
}

/*! \brief Create interval file name
 *
 * Create interval file name.
 *
 *  \param[out] path Complete path of the interval file
 *  \param fh Internal file handle
*/
static void
version_build_interval_path (string *path, internal_fh fh)
{
  path->str = xstrconcat (2, fh->version_path, VERSION_INTERVAL_FILE_ADD);
  path->len = strlen (path->str);
}

/*! \brief Load interval file
 *
 * Load interval file.
 *
 *  \param fh Internal file handle
*/
bool
version_load_interval_tree (internal_fh fh)
{
  int fd;
  string path;
  struct stat st;
  bool r = true;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  fh->version_interval_tree_users++;
  if (fh->version_interval_tree_users > 1)
    RETURN_BOOL (true);

  fh->versioned = interval_tree_create (1, NULL);

  version_build_interval_path (&path, fh);

  fd = open (path.str, O_RDONLY);
  if (fd < 0)
    {
      // detect that no interval file exists and we have complete file
      r = false;
    }
  else
    {
      if (fstat (fd, &st) < 0)
        {
          message (LOG_WARNING, FACILITY_DATA, "%s: fstat: %s\n", path.str, strerror (errno));
          r = false;
        }
      else if ((st.st_mode & S_IFMT) != S_IFREG)
        {
          message (LOG_WARNING, FACILITY_DATA, "%s: Not a regular file\n", path.str);
          r = false;
        }
      else if (st.st_size % sizeof (interval) != 0)
        {
          message (LOG_WARNING, FACILITY_DATA, "%s: Interval list is not aligned\n", path.str);
          r = false;
        }
      else
        {
          if (!interval_tree_read (fh->versioned, fd, st.st_size / sizeof (interval)))
            {
              interval_tree_destroy (fh->versioned);
              fh->versioned = NULL;
              r = false;
            }
        }
      close (fd);
    }

  free (path.str);

  RETURN_BOOL (r);
}

/*! \brief Write interval file
 *
 * Write interval file for the version associated with the current file.
 *
 *  \param fh Internal file handle
*/
bool
version_save_interval_trees (internal_fh fh)
{
  int fd;
  bool r = true;
  string path;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

#ifdef ENABLE_CHECKING
  if (fh->version_interval_tree_users == 0)
    abort ();
#endif

  fh->version_interval_tree_users--;
  if (fh->version_interval_tree_users > 0)
    RETURN_BOOL (true);

#ifdef ENABLE_CHECKING
  if (!fh->versioned)
    abort ();
#endif

  version_build_interval_path (&path, fh);

  fd = open (path.str, O_WRONLY | O_CREAT | O_TRUNC, fh->attr.mode);
  if (fd < 0)
    r = false;
  else if (!interval_tree_write (fh->versioned, fd))
    r = false;

  close (fd);
  free (path.str);

  interval_tree_destroy (fh->versioned);
  fh->versioned = NULL;

  RETURN_BOOL (r);
}

/*! \brief Generate file version specifier
 *
 * Add a version suffix to the specified file path. Suffix is generated from the current time.
 *
 *  \param path Complete file path.
 *  \param[out] verpath Complete file path including a version suffix.
*/
int32_t
version_generate_filename (char *path, string *verpath)
{
  time_t t;
  char stamp[VERSION_MAX_SPECIFIER_LENGTH]; // even unsigned long long int will fit in here

  // get current time
  if (time(&t) == -1)
    {
      message (LOG_WARNING, FACILITY_VERSION, "version_generate_filename: time returned error=%d\n", errno);
      RETURN_INT (errno);
    }

  // convert to string
  sprintf (stamp, "%ld", t);

  verpath->str = xstrconcat (3, path, VERSION_NAME_SPECIFIER_S, stamp);
  verpath->len = strlen (verpath->str);

  message (LOG_DEBUG, FACILITY_VERSION, "version_generate_filename: path=%s, stamp=%s\n", path, stamp);

  RETURN_INT (ZFS_OK);
}

/*! \brief Create version file
 *
 * Create version file based on the current file attributes.
 *
 *  \param path Complete file path
 *  \param dentry Dentry of the file
 *  \param with_size Whether version file should be enlarged to the size of the current file
*/
int32_t
version_create_file_with_attr (char *path, internal_dentry dentry, volume vol, bool with_size)
{
  fattr *sa;
  struct utimbuf t;

  zfs_fh fh;
  int32_t r;

  sa = &dentry->fh->attr;

  dentry->fh->version_fd = creat (path, GET_MODE (sa->mode));
  dentry->fh->version_path = xstrdup (path);

  if (lchown (path, map_uid_zfs2node (sa->uid),
              map_gid_zfs2node (sa->gid)) != 0)
    RETURN_INT (errno);

  t.actime = sa->atime;
  t.modtime = sa->mtime;
  if (utime (path, &t) != 0) RETURN_INT (errno);

  if (with_size)
    {
      if (truncate (path, sa->size) != 0) RETURN_INT (errno);
    }


  acquire_dentry (dentry->parent);

  r = ZFS_OK;

  if (r == ZFS_OK)
    {
      internal_dentry ndentry;
      zfs_fh master_fh;//, tmp_fh;
      string name;
      metadata meta;
      fattr attr;
      string spath;
      char *p;

      zfs_fh_undefine (master_fh);

      p = strrchr (path, '/');
      if (p) p++; else p = path;
      xmkstring(&name, p);

      xmkstring (&spath, path);
      r = local_getattr_path_ns (&attr, &spath);
      free (spath.str);

      fh.sid = dentry->fh->local_fh.sid;
      fh.vid = dentry->fh->local_fh.vid;
      fh.dev = attr.dev;
      fh.ino = attr.ino;
      meta.flags = METADATA_COMPLETE;
      meta.modetype = GET_MODETYPE (attr.mode, attr.type);
      meta.uid = attr.uid;
      meta.gid = attr.gid;
      lookup_metadata (vol, &fh, &meta, true);
      set_attr_version (&attr, &meta);

      ndentry = internal_dentry_create_ns (&fh, &master_fh, vol, dentry->parent, &name,
                                           sa, &meta, LEVEL_UNLOCKED);

      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
        {
          if (vol->master != this_node)
            {
              if (!add_journal_entry (vol, dentry->parent->fh->journal,
                                      &dentry->parent->fh->local_fh,
                                      &ndentry->fh->local_fh,
                                      &ndentry->fh->meta.master_fh,
                                      ndentry->fh->meta.master_version, &name,
                                      JOURNAL_OPERATION_ADD))
                {
                  MARK_VOLUME_DELETE (vol);
                }
            }
          if (!inc_local_version (vol, dentry->parent->fh))
            MARK_VOLUME_DELETE (vol);
        }
      release_dentry (ndentry);
      free (name.str);
    }

  release_dentry (dentry->parent);

  RETURN_INT (ZFS_OK);
}

/*! \brief Open version file
 *
 * Open version file for specified current file. If the appropriate version already exists, it is opened.
 * New version file is created otherwise.
 *
 *  \param denty Dentry of the file
 *  \param vol Volume of the file
 */
int32_t
version_create_file (internal_dentry dentry, volume vol)
{
  string path;
  string verpath;
  int r;
  struct stat st;

  message (LOG_DEBUG, FACILITY_VERSION, "version_create_file\n");

  build_local_path (&path, vol, dentry);
  version_generate_filename (path.str, &verpath);

  // is there any version we should use?
  r = lstat (verpath.str, &st);
  if (r == 0)
    {
      // open last version
      message (LOG_DEBUG, FACILITY_VERSION, "open last version\n");
      dentry->fh->version_fd = open (verpath.str, O_RDWR);
      dentry->fh->version_path = xstrdup (verpath.str);
    }
  else
    {
      version_create_file_with_attr (verpath.str, dentry, vol, true);
      version_apply_retention (dentry, vol);
    }

  free (verpath.str);
  free (path.str);

  version_load_interval_tree (dentry->fh);

  RETURN_INT (ZFS_OK);
}

/*! \brief Close version file
 *
 * Close version file of the specified current file.
 *
 *  \param fh Internal file handle of the file
 *  \param tidy Whether the function should make the version file sparse.
*/
int32_t
version_close_file (internal_fh fh, bool tidy)
{
  message (LOG_DEBUG, FACILITY_VERSION, "version_close_file: version_fd=%d\n", fh->version_fd);

  if (fh->version_fd < 0) RETURN_INT (ZFS_INVALID_REQUEST);

  close (fh->version_fd);
  fh->version_fd = -1;
  free (fh->version_path);
  fh->version_path = NULL;

  if (tidy)
    {
      if (WAS_FILE_TRUNCATED(fh))
        {
          //version_make_sparse();
          UNMARK_FILE_TRUNCATED(fh);
        }
    }

  RETURN_INT (ZFS_OK);
}

/*! \brief Perform file truncate with versioning
 *
 * Perform file truncate, i.e. rename current file to the version file and create new current file.
 *
 *  \param dentry Dentry of the file
 *  \param path Complete path of the file
*/
int32_t
version_truncate_file (internal_dentry dentry, volume vol, char *path)
{
  string verpath;

  // skip copy if version file already in use - it has all original attributes
  if (dentry->fh->version_fd > 0)
    RETURN_INT (ZFS_OK);

  version_generate_filename (path, &verpath);
  rename (path, verpath.str);
  free (verpath.str);
  version_create_file_with_attr (path, dentry, vol, false);
  MARK_FILE_TRUNCATED (dentry->fh);
  version_close_file (dentry->fh, false);

  RETURN_INT (ZFS_OK);
}

/*! \brief Perform file unlink with versioning
 *
 * Perform file unlink, i.e. rename current file to the version file.
 *
 *  \param path Complete path of the file
*/
int32_t
version_unlink_file (char *path)
{
  string verpath;
  struct stat st;

  // skip directories
  if (!lstat (path, &st) && S_ISDIR (st.st_mode))
    RETURN_INT (ZFS_OK);

  version_generate_filename (path, &verpath);
  rename (path, verpath.str);
  free (verpath.str);

  RETURN_INT (ZFS_OK);
}

/*! \brief Find version files for specified time stamp.
 *
 * Find version files that contain data for the specified time stamp, i.e. all newer version files
 * that combined together cover the whole file.
 *
 *  \param path Complete path of the directory
 *  \param name Name of the file
 *  \param stamp Time stamp of the moment in time
 *  \param[out] ino Inode of the first newer version file
 *  \param[out] v varray filled with list of versions
 *
 *  \see version_item_def
*/
int32_t
version_browse_dir (char *path, char *name, time_t *stamp, uint32_t *ino, varray *v)
{
  char buf[ZFS_MAXDATA];
  int32_t r, pos;
  struct dirent *de;
  int fd;
  int32_t cookie = 0;
  unsigned int nl;
  time_t res = 0;
  long current_ino = 0;

  nl = strlen(name);

  fd = open (path, O_RDONLY, 0);
  if (fd < 0)
    RETURN_INT (fd);

  r = lseek (fd, cookie, SEEK_SET);
  if (r < 0)
    {
      RETURN_INT (errno);
    }

  while (1)
    {
#ifdef __linux__
      r = getdents (fd, (struct dirent *) buf, ZFS_MAXDATA);
#else
      /* FIXME: make sure the buffer is => st_bufsiz */
      r = getdirentries (fd, buf, ZFS_MAXDATA, &block_start);
#endif
      if (r <= 0)
        {
          /* Comment from glibc: On some systems getdents fails with ENOENT
             when open directory has been rmdir'd already.  POSIX.1
             requires that we treat this condition like normal EOF.  */
          if (r < 0 && errno == ENOENT)
            r = 0;

          if (r == 0)
            {
              if (ino)
                {
                  if (res)
                    *stamp = res;
                  else
                    {
                      *ino = current_ino;
                      *stamp = 0;
                    }
                }
              RETURN_INT (ZFS_OK);
            }

          /* EINVAL means that buffer was too small.  */
          RETURN_INT (errno);
        }

      for (pos = 0; pos < r; pos += de->d_reclen)
        {
          de = (struct dirent *) &buf[pos];
#ifdef __linux__
          cookie = de->d_off;
#else
          /* Too bad FreeBSD doesn't provide that information, let's hope
             the kernel can handle slightly incorrect data. */
          cookie = block_start + pos + de->d_reclen;
#endif

          // check if we have our file
          if (!strncmp (de->d_name, name, nl))
            {
              // check if it is current file
              if (strlen (de->d_name) == nl)
                {
                  current_ino = de->d_ino;
                  if (v)
                    {
                      version_item item;
                      item.stamp = INT32_MAX;
                      item.name = xstrdup (de->d_name);
                      item.intervals = NULL;
                      item.path = NULL;
                      VARRAY_PUSH (*v, item, version_item);
                    }
                  continue;
                }

              // get name stamp
              char *p, *q;
              time_t t;

              p = strchr (de->d_name, VERSION_NAME_SPECIFIER_C);
              if (!p) continue;
              *p = '\0';
              p++;

              q = strchr (p, '.');
              if (q)
                {
                  *q = '\0';
                  q++;
                  // skip interval files
                  if (*q == 'i') continue;
                }
              // get version file stamp
              t = atoi (p);

              // compare timestamps if we are looking for a file
              if (ino && (t >= *stamp) && (!res || (t < res))) {
                res = t;
                *ino = de->d_ino;
              }

              // add into varray if we want list of version files
              if (v && (t > *stamp))
                {
                  version_item item;
                  *(--p) = VERSION_NAME_SPECIFIER_C;
                  item.stamp = t;
                  item.name = xstrdup (de->d_name);
                  item.intervals = NULL;
                  item.path = NULL;
                  VARRAY_PUSH (*v, item, version_item);
                }

            }
        }
    }
}

/*! \brief Find first newer version
 *
 * Find oldest version of the file that is newer than the specified time stamp.
 *
 *  \param dir Complete path of the directory
 *  \param[in,out] name File name; contains version suffix on return
 *  \param stamp Time stamp
*/
int32_t
version_find_version (char *dir, string *name, time_t stamp)
{
  char *sname;
  char *p;
  uint32_t ino = 0;
  struct stat st;
  char *x;
  char ver[VERSION_MAX_SPECIFIER_LENGTH];

  sname = xstrdup (name->str);
  p = strchr (sname, VERSION_NAME_SPECIFIER_C);
  if (p)
    *p = '\0';

  // check for exact version file
  snprintf (ver, sizeof(ver), "%ld", stamp);
  x = xstrconcat (5, dir, "/", sname, VERSION_NAME_SPECIFIER_S, ver);
  if (!stat (x, &st))
    {
      free (x);
      free (name->str);
      name->str = xstrconcat (3, sname, VERSION_NAME_SPECIFIER_S, ver);;
      name->len = strlen (name->str);
      free (sname);

      RETURN_INT (ZFS_OK);
    }
  free (x);

  // look for first newer
  version_browse_dir (dir, sname, &stamp, &ino, NULL);
  message (LOG_DEBUG, FACILITY_VERSION, "Using stamp=%d, sname=%s, ino=%u\n", stamp, sname, ino);

  if (stamp > 0)
    {
      // found version file
      free (name->str);
      snprintf (ver, sizeof(ver), "%ld", stamp);
      name->str = xstrconcat (3, sname, VERSION_NAME_SPECIFIER_S, ver);
      name->len = strlen (name->str);
    }
  else if (ino > 0)
    {
      // found current file
      p = strchr (name->str, VERSION_NAME_SPECIFIER_C);
      if (p) *p = '\0';
      name->len = strlen (name->str);
    }
  else
    {
      free (sname);
      RETURN_INT (ENOENT);
    }

  free (sname);

  RETURN_INT (ZFS_OK);
}

/*! Return part of the timestamp string.  */
#define GET_STAMP_PART(BUF, L, PART, S, E, X) \
  if (L > S) \
    { \
      if ((L > E) && (BUF[E] != '-')) RETURN_INT (ENOENT); \
      buf[E] = '\0'; \
      PART = atoi (BUF + S) + X; \
    }

/*! \brief Parse version suffix (datetime format)
 *
 * Parse time stamp version suffix from the file name. Function expects yyyy-mm-dd-hh-mm-ss format.
 *
 *  \param name File name with a version suffix
 *  \param[out] stamp Time stamp from the suffix
 *  \param[out] ornamelen Length of the file name without the version suffix
 *
 *  \retval ZFS_OK Version suffix was successfully parsed
 *  \retval ENOENT version suffix is not correct
*/
int32_t
version_get_filename_stamp(char *name, time_t *stamp, int *orgnamelen)
{
  char *x = name;

  *stamp = 0;
  if ((x = strchr (name, VERSION_NAME_SPECIFIER_C)))
    {
      struct tm tm;
      char buf[VERSION_MAX_SPECIFIER_LENGTH];
      unsigned int len;

      // check for @now timestamo
      if (!strcmp (x + 1, "now"))
        {
          time (stamp);
        }
      // check for @versions timestamp
      else if (!strcmp (x + 1, VERSION_LIST_VERSIONS_SUF))
        {
          *stamp = VERSION_LIST_VERSIONS_STAMP;
        }
      else
        {
          // extract timestamp
          memset (&tm, 0, sizeof (tm));
          // this code conforms to VERSION_TIMESTAMP
          memset (buf, 0, sizeof (buf));
          strncpy (buf, x + 1, sizeof (buf));
          len = strlen (buf);
          if (len > 19)
            {
              message (LOG_WARNING, FACILITY_VERSION, "Invalid version specifier: %s.\n", name);
              RETURN_INT (ENOENT);
            }

          GET_STAMP_PART (buf, len, tm.tm_year, 0, 4, -1900);
          GET_STAMP_PART (buf, len, tm.tm_mon, 5, 7, -1);
          GET_STAMP_PART (buf, len, tm.tm_mday, 8, 10, 0);
          GET_STAMP_PART (buf, len, tm.tm_hour, 11, 13, 0);
          GET_STAMP_PART (buf, len, tm.tm_min, 14, 16, 0);
          GET_STAMP_PART (buf, len, tm.tm_sec, 17, 19, 0);

          tm.tm_isdst = -1; // to handle daylight saving time
          *stamp = mktime (&tm);
          if (*stamp <= 0)
            {
              message (LOG_WARNING, FACILITY_VERSION, "Cannot convert tm struct to time.\n");
              RETURN_INT (ENOENT);
            }
        }

      message (LOG_DEBUG, FACILITY_VERSION, "Version stamp: %d\n", *stamp);
    }

  if (orgnamelen && x)
    *orgnamelen = x - name;

  RETURN_INT (ZFS_OK);
}


/*! \brief Return version time stamp (Unix time)
 *
 * Return time stamp from version file name. Function expects Unix time suffix.
 *
 *  \param name File name
 *  \param[out] stamp Time stamp value
 *
 *  \retval ZFS_OK Valid time stamp specified
 *  \retval ENOENT Invalid time stamp
*/
int32_t
version_retr_stamp(char *name, time_t *stamp)
{
  char *x;

  *stamp = 0;

  if (!(x = strchr (name, VERSION_NAME_SPECIFIER_C)))
    RETURN_INT (ENOENT);

  *stamp = atoi (x + 1);

  RETURN_INT (ZFS_OK);


}

/*! \brief Check if working with directory
 *
 * Check whether name is a directory.
 *
 *  \param[out] dst
 *  \param dir Parent directory name
 *  \param[in,out] name Child file/directory name
 *  \param stamp Time stamp applied to the parent directory
 *  \param ornamelen Length of the child file/directory name without the version suffix
 *
 *  \retval ZFS_OK Child is a directory; dst is filled in and name suffix is truncated
 *  \retval ENOENT Child in not a directory
*/
int32_t
version_is_directory (string *dst, char *dir, string *name, time_t stamp, time_t *dirstamp, int orgnamelen)
{
  char *x;
  struct stat st;

  x = xstrconcat (2, dir, name->str);
  if (orgnamelen) x[strlen(dir) + orgnamelen] = '\0';

  if (!lstat (x, &st) && S_ISDIR (st.st_mode))
    {
      // it is a directory
      dst->str = x;
      dst->len = strlen (dst->str);
      free (dir);

      if (orgnamelen)
        {
          if (dirstamp)
            *dirstamp = stamp;
          name->str[orgnamelen] = '\0';
          name->len = orgnamelen;
        }

      RETURN_INT (ZFS_OK);
    }

  free (x);

  RETURN_INT (ENOENT);
}

/*! \brief Compare
 *
 * Compare two version items. Function is used by Q-sort in version_build_intervals.
 *
 *  \param i1 First item
 *  \param i2 Second item
 *
 *  \retval -1 or 1 depending on which item has lower time stamp
 *
 *  \see version_item_def
*/
static int
comp_version_items (const void *i1, const void *i2)
{
  const version_item *x1 = (const version_item *)i1;
  const version_item *x2 = (const version_item *)i2;

  return x1->stamp < x2->stamp ? -1 : 1;
}

/*! \brief Create list of intervals for a file version
 *
 * Create list of intervals together with version files these intervals are stored in that create
 * the whole content of a file version. List is created from interval files during file open. Result is stored in
 * the internal file handle.
 *
 *  \param dentry Dentry of the file
 *  \param vol Volume the file in on
*/
int32_t
version_build_intervals (internal_dentry dentry, volume vol)
{
  time_t stamp;
  varray v;
  char *sname;
  char *p;
  version_item *list;
  unsigned int n, m;
  unsigned int i, j;
  string dpath;
  bool r;

  r = version_load_interval_tree (dentry->fh);
  if (!r)
    RETURN_INT (ZFS_OK);

  // parse version file name
  sname = xstrdup (dentry->name.str);
  p = strchr (sname, VERSION_NAME_SPECIFIER_C);
  if (!p)
    {
      free (sname);
      RETURN_INT (ENOENT);
    }

  *p = '\0';
  stamp = atoi (p + 1);

  // get list of version files that newer than timestamp
  zfsd_mutex_lock (&dentry->parent->fh->mutex);
  build_local_path (&dpath, vol, dentry->parent);
  zfsd_mutex_unlock (&dentry->parent->fh->mutex);

  varray_create (&v, sizeof(version_item), 1);

  version_browse_dir (dpath.str, sname, &stamp, NULL, &v);

  free (sname);

  n = VARRAY_USED (v);
  list = (version_item *) xmalloc (n * sizeof(version_item));

  for (i = 0; i < n; i++)
    {
      list[i] = VARRAY_ACCESS(v, i, version_item);
      list[i].path = xstrconcat (3, dpath.str, "/", list[i].name);
    }

  varray_destroy (&v);
  free (dpath.str);

  // sort the list
  qsort (list, n, sizeof (version_item), comp_version_items);

  // get intervals for version files in the list
  for (i = 0; i < n; i++)
    {
      //read
      int fd;
      char *ival;

      if (!list[i].path) continue;

      ival = xstrconcat (2, list[i].path, VERSION_INTERVAL_FILE_ADD);
      fd = open (ival, O_RDONLY);

      if (fd > 0)
        {
          // read interval file
          struct stat st;

          list[i].intervals = interval_tree_create (1, NULL);

          if (fstat (fd, &st) < 0)
            {
              CLEAR_VERSION_ITEM (list[i]);
            }
          else if (!interval_tree_read (list[i].intervals, fd, st.st_size / sizeof (interval)))
            CLEAR_VERSION_ITEM (list[i]);

          close (fd);
        }
      else if (errno == ENOENT)
        {
          // complete file
          i++;
          break;
        }
      // invalid version file
      else CLEAR_VERSION_ITEM (list[i]);

      free (ival);
    }

  // delete all redundant intervals
  m = i;
  for (j = i + 1; j < n; j++)
    CLEAR_VERSION_ITEM (list[j]);

  dentry->fh->version_list = list;
  dentry->fh->version_list_length = m;

  RETURN_INT (ZFS_OK);
}


/*! \brief
 *
 * Read data from the version file. It uses intervals build during open.
 *
 *  \param dentry Dentry of the file
 *  \param start Start of data interval
 *  \param end End of data interval
 *  \param[out] buf Buffer to store the data
 *
 *  \see version_build_intervals
*/
int32_t
version_read_old_data (internal_dentry dentry, uint64_t start, uint64_t end, char *buf)
{
  // read data from version files
  unsigned int i, j;
  version_item *item;
  interval_tree covered;
  uint32_t rsize = 0;

  covered = interval_tree_create(1, NULL);
  interval_tree_add (covered, dentry->fh->versioned);

  for (i = 0; i < dentry->fh->version_list_length; i++)
    {
      varray v, rv;
      int fd;

      item = &dentry->fh->version_list[i];
      if (!item->stamp) continue;

      if (item->intervals)
        interval_tree_intersection (item->intervals, start, end, &v);
      else
        {
          interval ival;
          ival.start = start;
          ival.end = end;
          varray_create(&v, sizeof (interval), 1);
          VARRAY_PUSH (v, ival, interval);
        }

      if (VARRAY_USED (v) == 0)
        {
          varray_destroy (&v);
          continue;
        }

      interval_tree_complement_varray(covered, &v, &rv);
      varray_destroy (&v);

      fd = open (item->path, O_RDONLY);
      for (j = 0; j < VARRAY_USED (rv); j++)
        {
          interval ival;
          ssize_t rd;

          ival = VARRAY_ACCESS (rv, j, interval);

          lseek (fd, ival.start, SEEK_SET);
          rd = read (fd, buf + ival.start - start, ival.end - ival.start);
          if (rd > 0)
            {
              rsize += rd;
              interval_tree_insert (covered, ival.start, ival.end);
            }
          message (LOG_DEBUG, FACILITY_VERSION, "read version name=%s, start=%lld, end=%lld, read=%d\n", item->name, ival.start, ival.end, rd);
        }
      close (fd);

      varray_destroy (&rv);
    }

  interval_tree_destroy (covered);

  RETURN_INT (ZFS_OK);
}

/*! \brief Create version during rename
 *
 * Create version file during file rename.
 *
 *  \param path Complete path of the file
*/
int32_t
version_rename_source(char *path)
{
  string verpath;
  int fd, fdv;
  struct stat st, stv;
  struct utimbuf t;
  int32_t r;

  // skip directories
  lstat (path, &st);
  if (S_ISDIR (st.st_mode))
    RETURN_INT (ZFS_OK);

  version_generate_filename (path, &verpath);
  // is there any version we should use?
  r = lstat (verpath.str, &stv);
  if (r == 0)
    {
      // open last version
      fdv = open (verpath.str, O_RDWR);
    }
  else
    {
      fdv = creat (verpath.str, st.st_mode);
      lchown (verpath.str, st.st_uid, st.st_gid);
      t.actime = st.st_atime;
      t.modtime = st.st_mtime;
      utime (verpath.str, &t);
    }
  free (verpath.str);

  fd = open (path, O_RDONLY);

  r = version_copy_data(fd, fdv, 0, st.st_size, NULL);

  close (fd);
  close (fdv);

  RETURN_INT (r);
}

/*! \brief Delete version file
 *
 * Delete version file and its respective interval file.
 *
 *  \param path Complete path of the file
*/
int32_t
version_unlink_version_file (char *path)
{
  int32_t r;
  char *x;

  // unlink both version file and interval file
  x = xstrconcat (2, path, VERSION_INTERVAL_FILE_ADD);
  unlink (x);
  free (x);

  r = unlink (path);

  RETURN_INT (r);
}

/*! \brief Delete version file
 *
 * Delete version file and its respective interval file.
 *
 *  \param dir Dentry of the directory the file is in
 *  \param vol Volume the file is on
 *  \param name Name of the file
*/
bool
version_retent_file (internal_dentry dir, volume vol, char *name)
{
  string path;
  char *dst;

  acquire_dentry (dir);
  zfsd_mutex_lock (&vol->mutex);
  zfsd_mutex_lock (&fh_mutex);

  build_local_path (&path, vol, dir);

  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  dst = xstrconcat (3, path.str, "/", name);

  version_unlink_version_file (dst);

  free (dst);

  // file is deleted immediately
  return true;
}

/*! \brief Copy data into version file
 *
 * Copy data from current file to version file. Copy only part of the file and only when it differs
 * from the data to be written.
 *
 *  \param fd File descriptor of current file
 *  \param fdv File descriptor of version file
 *  \param offset Start of data
 *  \param length Length of data
 *  \param newdata Buffer with data to be written (to compare it only, no write)
*/
int32_t
version_copy_data (int fd, int fdv, uint64_t offset, uint32_t length, data_buffer *newdata)
{
  TRACE ("");

  ssize_t r;
  char olddata[ZFS_VERSION_BLOCK_SIZE];
  off_t position = -1;
  uint32_t to_read;
  uint32_t to_write;

  message (LOG_DEBUG, FACILITY_VERSION, "version_copy_data: fd=%d, fdv=%d, offset=%lld, length=%d\n", fd, fdv, offset, length);

  if (fd < 0)
    {
      message (LOG_WARNING, FACILITY_VERSION, "old data open error: %d\n", -fd);
      RETURN_INT (-fd);
    }

  position = lseek (fd, offset, SEEK_SET);
  if (position == (off_t)-1)
    {
      message (LOG_WARNING, FACILITY_VERSION, "old data seek error: %d\n", errno);
      RETURN_INT (errno);
    }

  to_read = length;
  if (to_read >= sizeof(olddata)) to_read = sizeof(olddata) - 1;

  r = read (fd, olddata, to_read);
  if (r < 0)
    {
      message (LOG_WARNING, FACILITY_VERSION, "old data read error: %d\n", errno);
      RETURN_INT (errno);
    }

  if ((uint32_t)r < to_read)
    {
      message (LOG_WARNING, FACILITY_VERSION, "old data read requested: %d, read only: %d\n", to_read, r);
    }

  // compare to data to be written
  if (newdata && !memcmp(olddata, newdata, r))
    {
      message (LOG_DEBUG, FACILITY_VERSION, "new data same as old data, no version write\n");
      RETURN_INT (ZFS_OK);
    }

  // write data into version file
  position = lseek (fdv, offset, SEEK_SET);
  if (position == (off_t)-1)
    {
      message (LOG_WARNING, FACILITY_VERSION, "new data seek error: %d\n", errno);
      RETURN_INT (errno);
    }

  to_write = r;
  r = write (fdv, olddata, to_write);
  if (r < 0)
    {
      message (LOG_WARNING, FACILITY_VERSION, "new data write error: %d\n", errno);
      RETURN_INT (errno);
    }

  if ((uint32_t)r < to_write)
    {
      message (LOG_WARNING, FACILITY_VERSION, "new data write requested: %d, written only: %d\n", to_write, r);
    }

  RETURN_INT (ZFS_OK);
}

/*! \brief Remove all version files from a directory
 *
 * Remove all version files from specified directory. This function is called from rmdir
 * operations to make sure all files are deleted even version files are not displayed.
 * Works correctly only if there are no other files but versions.
 *
 *  \param path Complete path to the directory
*/
int32_t
version_rmdir_versions (char *path)
{
  char buf[ZFS_MAXDATA];
  int32_t r, pos;
  struct dirent *de;
  int fd;
  int working = 1;

  fd = open (path, O_RDONLY, 0);
  if (fd < 0)
    RETURN_INT (fd);

  while (working)
    {
      /* Always start from the beginning. We are modifying the contents of the directory.
       * And deleting files, so we will continue until there are no (version) files.
       */
      r = lseek (fd, 0, SEEK_SET);
      if (r < 0) break;

      /* Comments to the work with getdents can be found in other functions.  */
#ifdef __linux__
      r = getdents (fd, (struct dirent *) buf, ZFS_MAXDATA);
#else
      r = getdirentries (fd, buf, ZFS_MAXDATA, &block_start);
#endif
      if (r <= 0)
        {
          if (r < 0 && errno == ENOENT)
            break;
          RETURN_INT (errno);
        }

      /* If we delete at least one file, we should start over again.  */
      working = 0;
      for (pos = 0; pos < r; pos += de->d_reclen)
        {
          de = (struct dirent *) &buf[pos];

          /* Hide version files or convert their names or select them for storage.  */
          if (strchr (de->d_name, VERSION_NAME_SPECIFIER_C))
            {
              char *f;
              f = xstrconcat (3, path, "/", de->d_name);
              unlink (f);
              free (f);
              working = 1;
            }

        }
    }

  close (fd);

  RETURN_INT (ZFS_OK);
}

int32_t
version_apply_retention (internal_dentry dentry, volume vol)
{
  string dpath;
  char buf[ZFS_MAXDATA];
  int32_t r, pos;
  struct dirent *de;
  int fd;

  acquire_dentry (dentry->parent);
  build_local_path (&dpath, vol, dentry->parent);
  release_dentry (dentry->parent);

  fd = open (dpath.str, O_RDONLY, 0);
  if (fd < 0)
    {
      free (dpath.str);
      RETURN_INT (fd);
    }

  r = lseek (fd, 0, SEEK_SET);

  /* Create a list of all versions for specified file.  */
  while (0)
    {
      /* Comments to the work with getdents can be found in other functions.  */
#ifdef __linux__
      r = getdents (fd, (struct dirent *) buf, ZFS_MAXDATA);
#else
      r = getdirentries (fd, buf, ZFS_MAXDATA, &block_start);
#endif
      if (r <= 0)
        {
          if (r < 0 && errno == ENOENT)
            break;
          RETURN_INT (errno);
        }

      for (pos = 0; pos < r; pos += de->d_reclen)
        {
          de = (struct dirent *) &buf[pos];

        }
    }

  close (fd);

  /* Sort versions.  */

  /* Process from the oldest and check if it violates any maximum, while complying with minimums.
   * Delete such versions.
   */

  //xstrconcat(3, dpath.str, "/", name);
  //unlink()

  free (dpath.str);

  RETURN_INT (ZFS_OK);
}

#endif /* VERSIONS */
