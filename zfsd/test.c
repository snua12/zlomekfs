/* Test ZFS.
   Copyright (C) 2003 Josef Zlomek

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

/* Testing configuration until configuration reading is programmed.  */

void
fake_config ()
{
  node nod;
  volume vol;

  get_node_name ();
  set_string (&kernel_file_name, "/home/joe/kernel");

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (1, "orion");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (1);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir1", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume1", "/volume1", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (2);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume2", "/volume2", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (2, "sabbath");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (3);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir1", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume3", "/volume1/volume3", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (4);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume4", "/volume2/sabbath/volume4", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (3, "jaro");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (5);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/home/joe/.zfs/dir1", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume5", "/jaro/volume5", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (6);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/home/joe/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume6", "/volume6", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  debug_virtual_tree ();
}

/* Test splay tree data type.  */

#include "splay-tree.h"
static void
test_splay ()
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
test_interval ()
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

static int
walk_dir (zfs_fh *dir, char *path)
{
  zfs_cap cap;
  dir_op_res res;
  DC dc;
  int32_t r;

  if (!get_running ())
    return ZFS_EXITING;

  r = zfs_open (&cap, dir, O_RDONLY);
  if (r == ZFS_OK)
    {
      int cookie = 0;
      dir_list list;
      unsigned int i;
      direction dddd;
      uint32_t rid;

      message (0, stderr, "%s\n", path);
      dc_create (&dc, ZFS_MAX_REQUEST_LEN);

      do {
	if (!get_running ())
	  return ZFS_EXITING;

	start_encoding (&dc);
	encode_direction (&dc, DIR_REPLY);
	encode_request_id (&dc, 1234567890);
	r = zfs_readdir (&dc, &cap, cookie, ZFS_MAXDATA);
	finish_encoding (&dc);
	if (r != ZFS_OK)
	  {
	    if (r > 0)
	      message (0, stderr, "readdir(): %d (%s)\n", r, strerror (r));
	    else
	      message (0, stderr, "readdir(): %d\n", r);
	    zfs_close (&cap);
	    dc_destroy (&dc);
	    return r;
	  }
	start_decoding (&dc);
	if (!decode_direction (&dc, &dddd)
	    || !decode_request_id (&dc, &rid)
	    || !decode_status (&dc, &r)
	    || !decode_dir_list (&dc, &list))
	  {
	    zfs_close (&cap);
	    dc_destroy (&dc);
	    return ZFS_INVALID_REPLY;
	  }

	for (i = 0; i < list.n; i++)
	  {
	    dir_entry entry;

	    if (!decode_dir_entry (&dc, &entry))
	      {
		zfs_close (&cap);
		dc_destroy (&dc);
		return ZFS_INVALID_REPLY;
	      }

	    cookie = entry.cookie;
	    if (entry.name.str[0] == '.'
		&& (entry.name.str[1] == 0
		    || (entry.name.str[1] == '.'
			&& entry.name.str[2] == 0)))
	      {
		free (entry.name.str);
		continue;
	      }

	    if (!get_running ())
	      {
		free (entry.name.str);
		return ZFS_EXITING;
	      }

	    r = zfs_lookup (&res, dir, &entry.name);
	    if (r != ZFS_OK)
	      {
		if (r > 0)
		  message (0, stderr, "lookup(): %d (%s)\n", r, strerror (r));
		else
		  message (0, stderr, "lookup(): %d\n", r);
		free (entry.name.str);
		continue;
	      }
	    if (res.attr.type == FT_DIR)
	      {
		char *new_path;

		new_path = xstrconcat (3, path, entry.name.str, "/");
		r = walk_dir (&res.file, new_path);
		free (new_path);

		if (!get_running ())
		  {
		    free (entry.name.str);
		    return ZFS_EXITING;
		  }
	      }
	    else
	      message (0, stderr, "%s%s\n", path, entry.name.str);
	    free (entry.name.str);
	  }
      } while (list.eof == 0);
      zfs_close (&cap);
      dc_destroy (&dc);
    }
  else
    {
      if (r > 0)
	message (0, stderr, "open(): %d (%s)\n", r, strerror (r));
      else
	message (0, stderr, "open(): %d\n", r);
    }

  return r;
}

/* Test functions accessing ZFS.  */

static void*
do_tests (void *data)
{
  dir_op_res res;
  dir_op_res res2;
  zfs_cap cap;
  string rmdir_name = {3, "dir"};
  sattr sa = {0755, (unsigned int) -1, (unsigned int) -1, (uint64_t) -1, (zfs_time) -1, (zfs_time) -1};
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

  thread_disable_signals ();
  pthread_setspecific (thread_data_key, data);

  if (0)
    {
      test_splay ();
      test_interval ();
    }

  do
    {
      node nod;
      char *str;
      int r;

      if (!get_running ())
	break;

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST NULL\n");
      r = zfs_proc_null_client (t, NULL, nod);

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      if (r >= ZFS_ERROR_HAS_DC_REPLY)
	recycle_dc_to_node (&t->dc_reply, nod);
      zfsd_mutex_unlock (&nod->mutex);

      if (!get_running ())
	break;

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST ROOT\n");
      r = zfs_proc_root_client (t, NULL, nod);

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      if (r >= ZFS_ERROR_HAS_DC_REPLY)
	recycle_dc_to_node (&t->dc_reply, nod);
      zfsd_mutex_unlock (&nod->mutex);

      if (!get_running ())
	break;

      message (2, stderr, "TEST LOOKUP /volume2/sabbath/hidden\n");
      str = xstrdup ("/volume2/sabbath/hidden");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST LOOKUP /volume1/subdir/file\n");
      str = xstrdup ("/volume1/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST LOOKUP /volume1/volume3/subdir/file\n");
      str = xstrdup ("/volume1/volume3/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST LOOKUP /volume1/volume3/subdir\n");
      str = xstrdup ("/volume1/volume3/subdir");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST MKDIR\n");
      printf ("%d\n", zfs_mkdir (&res2, &res.file, &rmdir_name, &sa));

      if (!get_running ())
	break;

      message (2, stderr, "TEST RMDIR\n");
      printf ("%d\n", zfs_rmdir (&res.file, &rmdir_name));

      if (!get_running ())
	break;

      message (2, stderr, "TEST CREATE\n");
      printf ("%d\n", r = zfs_create (&creater, &res.file, &test,
				      O_RDWR | O_TRUNC | O_CREAT, &sa));

      if (r == ZFS_OK)
	{
	  if (!get_running ())
	    break;

	  message (2, stderr, "TEST CLOSE\n");
	  printf ("%d\n", zfs_close (&cap));
	}

      message (2, stderr, "TEST LINK\n");
      printf ("%d\n", zfs_link (&cap.fh, &res.file, &test2));

      message (2, stderr, "TEST UNLINK\n");
      printf ("%d\n", zfs_unlink (&res.file, &test));

      message (2, stderr, "TEST RENAME\n");
      printf ("%d\n", zfs_rename (&res.file, &test2, &res.file, &test3));

      message (2, stderr, "TEST UNLINK\n");
      printf ("%d\n", zfs_unlink (&res.file, &test3));

      message (2, stderr, "TEST SYMLINK\n");
      printf ("%d\n", zfs_symlink (&res.file, &sym, &path, &sa));

      message (2, stderr, "TEST LOOKUP /volume1/volume3/subdir/symlink\n");
      str = xstrdup ("/volume1/volume3/subdir/symlink");
      printf ("%d\n", zfs_extended_lookup (&res2, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST READLINK\n");
      printf ("%d\n", zfs_readlink (&readlinkr, &res2.file));

      message (2, stderr, "TEST UNLINK\n");
      printf ("%d\n", zfs_unlink (&res.file, &sym));

      message (2, stderr, "TEST MKNOD\n");
      printf ("%d\n", zfs_mknod (&res.file, &pip, &sa, FT_FIFO, 1234));

      message (2, stderr, "TEST UNLINK\n");
      printf ("%d\n", zfs_unlink (&res.file, &pip));

      if (!get_running ())
	break;

      message (2, stderr, "TEST LOOKUP /volume1/volume3/subdir/file\n");
      str = xstrdup ("/volume1/volume3/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      if (!get_running ())
	break;

      message (2, stderr, "TEST OPEN\n");
      printf ("%d\n", r = zfs_open (&cap, &res.file, O_RDWR));

      if (r == ZFS_OK)
	{
	  write_args writea;
	  write_res writer;
	  uint32_t request_id;
	  direction dir;
	  int32_t status;
	  data_buffer data;
	  DC dc;

	  writea.cap = cap;
	  writea.offset = 0;
	  writea.data.len = 4;
	  memcpy (writea.data.buf, "abcd", writea.data.len);
	  message (2, stderr, "TEST WRITE\n");
	  printf ("%d", zfs_write (&writer, &writea));
	  printf (" %d\n", writer.written);

	  if (!get_running ())
	    break;

	  dc_create (&dc, ZFS_MAX_REQUEST_LEN);
	  start_encoding (&dc);
	  encode_direction (&dc, DIR_REPLY);
	  encode_request_id (&dc, 0);
	  message (2, stderr, "TEST READ\n");
	  printf ("%d\n", zfs_read (&dc, &cap, 0, 4));
	  finish_encoding (&dc);
	  start_decoding (&dc);
	  decode_direction (&dc, &dir);
	  decode_request_id (&dc, &request_id);
	  decode_status (&dc, &status);
	  decode_data_buffer (&dc, &data);
	  if (data.len != 4 || memcmp (data.buf, "abcd", 4) != 0)
	    printf ("FAILURE\n");

	  if (!get_running ())
	    break;

	  message (2, stderr, "TEST CLOSE\n");
	  printf ("%d\n", zfs_close (&cap));
	}

      if (!get_running ())
	break;

      message (2, stderr, "TEST SETATTR\n");
      printf ("%d\n", zfs_setattr (&fa, &res.file, &sa));

      if (!get_running ())
	break;

      if (0)
	{
	  message (0, stderr, "Walking through directory structure:\n");
	  walk_dir (&root_fh, "/");
	}
    }
  while (0);

  message (2, stderr, "TESTS FINISHED\n");
  return data;
}

/* Create a thread which tests ZFS.  */

void
test_zfs ()
{
  if (get_running ()
      && strcmp (node_name, "orion") == 0)
    {
      pthread_t id;

      /* Initialize testing thread data.  */
      semaphore_init (&testing_thread_data.sem, 0);
      network_worker_init (&testing_thread_data);

      if (pthread_create (&id, NULL, do_tests, &testing_thread_data))
	message (-1, stderr, "pthread_create() failed\n");
      else
	pthread_join (id, NULL);

      /* Destroy main thread data.  */
      network_worker_cleanup (&testing_thread_data);
      semaphore_destroy (&testing_thread_data.sem);
    }
}
