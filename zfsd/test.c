/* Test ZFS.
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "pthread.h"
#include "test.h"
#include "constant.h"
#include "memory.h"
#include "log.h"
#include "config.h"
#include "node.h"
#include "volume.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "thread.h"
#include "network.h"
#include "zfs_prot.h"

/* Data for testing thread.  */
static thread testing_thread_data;

/* ID of test thread.  */
static pthread_t test_id;

/* Testing configuration until configuration reading is programmed.  */

void
fake_config (void)
{
  node nod;
  volume vol;

  set_node_name ();
  set_default_uid_gid ();
  set_string (&kernel_file_name, "/dev/zfs");

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (1, "orion");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (1);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume1", "/volume1", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/.zfs/dir1", VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (2);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume2", "/volume2", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/.zfs/dir2", VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (2, "artax");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (3);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume3", "/volume1/volume3", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/home/zlomj9am/.zfs/dir1",
				 VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
#ifdef TEST_UPDATE
  else if (this_node && strcmp (this_node->name, "orion") == 0)
    /* FIXME: race condition?  */
    {
      if (volume_set_local_info (vol, "/.zfs/vol3", VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
#endif
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (4);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume4", "/volume2/artax/volume4", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/home/zlomj9am/.zfs/dir2",
				 VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (3, "find");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (5);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume5", "/other/volume5", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/home/joe/.zfs/dir1", VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (6);
  zfsd_mutex_unlock (&volume_mutex);
  volume_set_common_info (vol, "volume6", "/volume6", nod);
  if (nod == this_node)
    {
      if (volume_set_local_info (vol, "/home/joe/.zfs/dir2", VOLUME_NO_LIMIT))
	zfsd_mutex_unlock (&vol->mutex);
      else
	volume_delete (vol);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&nod->mutex);

  debug_virtual_tree ();
}

/* Test splay tree data type.  */

#include "splay-tree.h"
static void
test_splay (void)
{
  splay_tree st;
  int i;

  st = splay_tree_create (200, NULL, NULL);
  for (i = 0; i <= 4; i++)
    splay_tree_insert (st, 2 * i, i);
  splay_tree_lookup (st, 7);
  debug_splay_tree (st);
}

/* Test interval  tree data type.  */

#include "interval.h"
static void
test_interval (void)
{
  interval_tree t;

  t = interval_tree_create (6, NULL);
  interval_tree_insert (t, 0, 4);
  interval_tree_insert (t, 10, 15);
  interval_tree_insert (t, 20, 25);
  interval_tree_insert (t, 30, 32);
  interval_tree_insert (t, 40, 45);
  interval_tree_insert (t, 50, 55);
  interval_tree_insert (t, 60, 65);
  debug_interval_tree (t);
}

/* Print contents of directory DIR (using PATH as a prefix)
   and walk through subdirectories.  */

static int32_t
walk_dir (zfs_fh *dir, char *path)
{
  zfs_cap cap;
  dir_op_res res;
  int32_t r;
  uint32_t i;
  int32_t cookie;
  dir_list list;
  dir_entry entries[ZFS_MAX_DIR_ENTRIES];

  if (!get_running ())
    return ZFS_EXITING;

  r = zfs_open (&cap, dir, O_RDONLY);
  if (r == ZFS_OK)
    {
      message (0, stderr, "%s\n", path);
      cookie = 0;

      do {
	if (!get_running ())
	  return ZFS_EXITING;

	list.n = 0;
	list.eof = 0;
	list.buffer = entries;

	r = zfs_readdir (&list, &cap, cookie, ZFS_MAXDATA, &filldir_array);
	if (r != ZFS_OK)
	  {
	    message (0, stderr, "readdir(): %d (%s)\n", r, zfs_strerror (r));
	    zfs_close (&cap);
	    return r;
	  }

	for (i = 0; i < list.n; i++)
	  {
	    cookie = entries[i].cookie;
	    if (entries[i].name.str[0] == '.'
		&& (entries[i].name.str[1] == 0
		    || (entries[i].name.str[1] == '.'
			&& entries[i].name.str[2] == 0)))
	      {
		free (entries[i].name.str);
		continue;
	      }

	    if (!get_running ())
	      goto walk_dir_exiting;

	    r = zfs_lookup (&res, dir, &entries[i].name);
	    if (r != ZFS_OK)
	      {
		message (0, stderr, "lookup(): %d (%s)\n", r, zfs_strerror (r));
		free (entries[i].name.str);
		continue;
	      }
	    if (res.attr.type == FT_DIR)
	      {
		char *new_path;

		new_path = xstrconcat (3, path, entries[i].name.str, "/");
		r = walk_dir (&res.file, new_path);
		free (new_path);

		if (!get_running ())
		  goto walk_dir_exiting;
	      }
	    else
	      message (0, stderr, "%s%s\n", path, entries[i].name.str);
	    free (entries[i].name.str);
	  }
      } while (list.eof == 0);

      r = zfs_close (&cap);
      if (r != ZFS_OK)
	message (0, stderr, "close(): %d (%s)\n", r, zfs_strerror (r));
    }
  else
    message (0, stderr, "open(): %d (%s)\n", r, zfs_strerror (r));

  return r;

walk_dir_exiting:
  for (; i < list.n; i++)
    free (entries[i].name.str);
  return ZFS_EXITING;
}

/* Test functions accessing ZFS.  */

static void*
do_tests (void *data)
{
  dir_op_res res;
  dir_op_res res2;
  zfs_cap cap;
  string rmdir_name = {3, "dir"};
  sattr sa = {0755, (uint32_t) -1, (uint32_t) -1, (uint64_t) -1, (zfs_time) -1, (zfs_time) -1};
  sattr sa_symlink = {(uint32_t) -1, (uint32_t) -1, (uint32_t) -1, (uint64_t) -1, (zfs_time) -1, (zfs_time) -1};
  fattr fa;
  thread *t = (thread *) data;
  create_res creater;
  read_link_res readlinkr;
  string test = {4, "test"};
  string test2 = {5, "test2"};
  string test3 = {5, "test3"};
  string sym = {7, "symlink"};
  string path = {4, "path"};
  string pip = {4, "pipe"};
  data_buffer ping = {5, "abcde" }, ping_res;
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  char buffer[ZFS_MAXDATA];

  thread_disable_signals ();
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Testing thread");
  set_lock_info (li);

  if (0)
    {
      test_splay ();
      test_interval ();
    }

  if (1)
    {
      node nod;
      char *str;
      int32_t r;
      int fd;

      if (!get_running ())
	goto out;

      nod = node_lookup (2);
      message (1, stderr, "TEST NULL\n");
      r = zfs_proc_null_client (t, NULL, nod, &fd);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      if (r >= ZFS_ERROR_HAS_DC_REPLY)
	recycle_dc_to_fd (t->dc_reply, fd);

      if (!get_running ())
	goto out;

      nod = node_lookup (2);
      message (1, stderr, "TEST PING\n");
      r = zfs_proc_ping_client (t, &ping, nod, &fd);
      if (r == ZFS_OK)
	{
	  if (!decode_data_buffer (t->dc_reply, &ping_res)
	      || !finish_decoding (t->dc_reply))
	    message (1, stderr, "  INVALID_REPLY\n");
	  else if (ping.len != ping_res.len
	      || strncmp (ping.buf, ping_res.buf, ping.len) != 0)
	    message (1, stderr, "  MISCOMPARE\n");
	}
      message (1, stderr, "  %s\n", zfs_strerror (r));
      if (r >= ZFS_ERROR_HAS_DC_REPLY)
	recycle_dc_to_fd (t->dc_reply, fd);

      if (!get_running ())
	goto out;

      nod = node_lookup (2);
      message (1, stderr, "TEST ROOT\n");
      r = zfs_proc_root_client (t, NULL, nod, &fd);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      if (r >= ZFS_ERROR_HAS_DC_REPLY)
	recycle_dc_to_fd (t->dc_reply, fd);

      if (!get_running ())
	goto out;

      message (1, stderr, "TEST LOOKUP /volume2/artax/hidden\n");
      str = xstrdup ("/volume2/artax/hidden");
      r = zfs_extended_lookup (&res, &root_fh, str);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      free (str);

      if (!get_running ())
	goto out;

      message (1, stderr, "TEST LOOKUP /volume1/subdir/file\n");
      str = xstrdup ("/volume1/subdir/file");
      r = zfs_extended_lookup (&res, &root_fh, str);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      free (str);

      if (!get_running ())
	goto out;

      message (1, stderr, "TEST LOOKUP /volume1/volume3/subdir/file\n");
      str = xstrdup ("/volume1/volume3/subdir/file");
      r = zfs_extended_lookup (&res, &root_fh, str);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      free (str);

      if (!get_running ())
	goto out;

      message (1, stderr, "TEST LOOKUP /volume1/volume3/subdir\n");
      str = xstrdup ("/volume1/volume3/subdir");
      r = zfs_extended_lookup (&res, &root_fh, str);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      free (str);

      if (!get_running ())
	goto out;

      if (r == ZFS_OK)
	{
	  message (1, stderr, "TEST MKDIR\n");
	  r = zfs_mkdir (&res2, &res.file, &rmdir_name, &sa);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST RMDIR\n");
	  r = zfs_rmdir (&res.file, &rmdir_name);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST CREATE\n");
	  r = zfs_create (&creater, &res.file, &test,
			  O_RDWR | O_TRUNC | O_CREAT, &sa);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  if (r == ZFS_OK)
	    {
	      message (1, stderr, "TEST CLOSE\n");
	      r = zfs_close (&creater.cap);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST LINK\n");
	      r = zfs_link (&creater.cap.fh, &res.file, &test2);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST UNLINK\n");
	      r = zfs_unlink (&res.file, &test);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST RENAME\n");
	      r = zfs_rename (&res.file, &test2, &res.file, &test3);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST UNLINK\n");
	      r = zfs_unlink (&res.file, &test3);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;
	    }

	  message (1, stderr, "TEST SYMLINK\n");
	  r = zfs_symlink (&res2, &res.file, &sym, &path, &sa_symlink);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST LOOKUP /volume1/volume3/subdir/symlink\n");
	  str = xstrdup ("/volume1/volume3/subdir/symlink");
	  r = zfs_extended_lookup (&res2, &root_fh, str);
	  message (1, stderr, "  %s\n", zfs_strerror (r));
	  free (str);

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST READLINK\n");
	  r = zfs_readlink (&readlinkr, &res2.file);
	  message (1, stderr, "  %s\n", zfs_strerror (r));
	  if (r == ZFS_OK)
	    free (readlinkr.path.str);

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST UNLINK\n");
	  r = zfs_unlink (&res.file, &sym);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST MKNOD\n");
	  r = zfs_mknod (&res2, &res.file, &pip, &sa, FT_FIFO, 1234);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST UNLINK\n");
	  r = zfs_unlink (&res.file, &pip);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;
	}

      message (1, stderr, "TEST LOOKUP /volume1/volume3/subdir/file\n");
      str = xstrdup ("/volume1/volume3/subdir/file");
      r = zfs_extended_lookup (&res, &root_fh, str);
      message (1, stderr, "  %s\n", zfs_strerror (r));
      free (str);

      if (!get_running ())
	goto out;

      if (r == ZFS_OK)
	{
	  message (1, stderr, "TEST OPEN\n");
	  r = zfs_open (&cap, &res.file, O_RDWR);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  if (r == ZFS_OK)
	    {
	      write_args writea;
	      write_res writer;
	      data_buffer data;

	      writea.data.buf = buffer;
	      data.buf = buffer;

	      message (1, stderr, "TEST READ\n");
	      r = zfs_read (&data.len, data.buf, &cap, 16, 16, true);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;

	      writea.cap = cap;
	      writea.offset = 0;
	      writea.data.len = 4;
	      memcpy (writea.data.buf, "abcd", writea.data.len);
	      message (1, stderr, "TEST WRITE\n");
	      r = zfs_write (&writer, &writea);
	      message (1, stderr, "  %s\n", zfs_strerror (r));
	      if (r == ZFS_OK)
		message (1, stderr, "  %d\n", writer.written);

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST READ\n");
	      r = zfs_read (&data.len, data.buf, &cap, 0, 4, true);
	      message (1, stderr, "  %s\n", zfs_strerror (r));
	      if (r == ZFS_OK
		  && (data.len != 4 || memcmp (data.buf, "abcd", 4) != 0))
		message (1, stderr, "FAILURE\n");

	      if (!get_running ())
		goto out;

	      message (1, stderr, "TEST CLOSE\n");
	      r = zfs_close (&cap);
	      message (1, stderr, "  %s\n", zfs_strerror (r));

	      if (!get_running ())
		goto out;
	    }

	  message (1, stderr, "TEST SETATTR\n");
	  r = zfs_setattr (&fa, &res.file, &sa);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;

	  message (1, stderr, "TEST GETATTR\n");
	  r = zfs_getattr (&fa, &res.file);
	  message (1, stderr, "  %s\n", zfs_strerror (r));

	  if (!get_running ())
	    goto out;
	}
    }

  if (1)
    {
      message (0, stderr, "Walking through directory structure:\n");
      walk_dir (&root_fh, "/");
    }

out:
  message (1, stderr, "TESTS FINISHED\n");
  return NULL;
}

/* Create a thread which tests ZFS.  */

void
test_zfs (void)
{
  if (get_running ()
      && strcmp (node_name.str, "orion") == 0)
    {
      /* Initialize testing thread data.  */
      semaphore_init (&testing_thread_data.sem, 0);
      network_worker_init (&testing_thread_data);
      testing_thread_data.from_sid = this_node->id;

      if (pthread_create (&test_id, NULL, do_tests, &testing_thread_data))
	{
	  message (-1, stderr, "pthread_create() failed\n");
	  test_id = 0;
	}
    }
}

/* Cleanup after tests.  */

void
test_cleanup ()
{
  if (test_id)
    pthread_join (test_id, NULL);

  /* Destroy test thread data.  */
  network_worker_cleanup (&testing_thread_data);
  semaphore_destroy (&testing_thread_data.sem);
}
