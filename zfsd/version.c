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

static void
version_build_interval_path (string *path, internal_fh fh)
{
  path->str = xstrconcat (2, fh->version_path, ".i");
  path->len = strlen (path->str);
}

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

  version_build_interval_path (&path, fh);

  fd = open (path.str, O_RDONLY);
  if (fd < 0)
    {
      if (errno != ENOENT)
        r = false;
      else
        fh->versioned = interval_tree_create (1, &fh->mutex);
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
          fh->versioned = interval_tree_create (1, &fh->mutex);
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

  RETURN_BOOL (r);
}

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

int32_t
version_create_file_with_attr (char *path, internal_dentry dentry, bool with_size)
{
  fattr *sa;
  struct utimbuf t;

  message (LOG_DEBUG, FACILITY_VERSION, "version_create_file_with_attr: path=%s, with_size=%d\n", path, with_size);

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

  RETURN_INT (ZFS_OK);
}

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
      version_create_file_with_attr (verpath.str, dentry, true);
    }

  free (verpath.str);
  free (path.str);

  version_load_interval_tree (dentry->fh);

  RETURN_INT (ZFS_OK);
}

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

int32_t
version_truncate_file (internal_dentry dentry, char *path)
{
  string verpath;

  version_generate_filename (path, &verpath);
  rename (path, verpath.str);
  free (verpath.str);
  version_create_file_with_attr (path, dentry, false);
  MARK_FILE_TRUNCATED (dentry->fh);
  version_close_file (dentry->fh, false);

  RETURN_INT (ZFS_OK);
}

int32_t
version_unlink_file (char *path)
{
  string verpath;

  version_generate_filename (path, &verpath);
  rename (path, verpath.str);
  free (verpath.str);

  RETURN_INT (ZFS_OK);
}

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
              //printf("found file, name=%s\n", de->d_name);

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
  if (!p)
    {
      free (sname);
      RETURN_INT (ENOENT);
    }

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

  free (sname);

  RETURN_INT (ZFS_OK);
}

int32_t
version_get_filename_stamp(char *name, time_t *stamp)
{
  char *x;

  *stamp = 0;
  if ((x = strchr (name, VERSION_NAME_SPECIFIER_C)))
    {
      struct tm tm;
      char *s;

      // extract timestamp
      s = strptime (x + 1, "%Y-%m-%d-%H-%M-%S", &tm);
      if (!s || *s)
        {
          message (LOG_WARNING, FACILITY_VERSION, "Cannot convert version specifier to datetime: %s\n", x + 1);
          RETURN_INT (ENOENT);
        }

      tm.tm_isdst = 1; // to handle daylight saving time
      *stamp = mktime (&tm);
      if (*stamp < 0)
        {
          message (LOG_WARNING, FACILITY_VERSION, "Cannot convert tm struct to time.\n");
          RETURN_INT (ENOENT);
        }

      message (LOG_DEBUG, FACILITY_VERSION, "Version stamp: %d\n", *stamp);
    }

  RETURN_INT (ZFS_OK);
}

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

static int
comp_version_items (const void *i1, const void *i2)
{
  const version_item *x1 = (const version_item *)i1;
  const version_item *x2 = (const version_item *)i2;

  return x1->stamp < x2->stamp ? -1 : 1;
}

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

  version_load_interval_tree (dentry->fh);
  if (dentry->fh->versioned->size == 0)
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

      ival = xstrconcat (2, list[i].path, ".i");
      fd = open (ival, O_RDONLY);

      if (fd > 0)
        {
          // read interval file
          struct stat st;

          list[i].intervals = interval_tree_create (1, &dentry->fh->mutex);

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

int32_t
version_read_old_data (internal_dentry dentry, uint64_t start, uint64_t end, char *buf)
{
  // read data from version files
  unsigned int i, j;
  version_item *item;
  interval_tree covered;
  uint32_t rsize = 0;

  covered = interval_tree_create(1, &dentry->fh->mutex);
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

/*! \brief xxx
 *
 * xxx
 *
 *  \param xxx xxx
 *
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

#endif /* VERSIONS */
