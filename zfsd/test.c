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
#include "pthread.h"
#include "test.h"
#include "memory.h"
#include "log.h"
#include "config.h"
#include "node.h"
#include "volume.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "thread.h"
#include "zfs_prot.h"

/* Testing configuration until configuration reading is programmed.  */

void
fake_config ()
{
  node nod;
  volume vol;

  get_node_name ();
  set_string (&kernel_file_name, "/.zfs/kernel");

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
    volume_set_local_info (vol, "/home/joe/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume5", "/jaro/volume5", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (6);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "home/joe/.zfs/dir2", VOLUME_NO_LIMIT);
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

/* Test functions accessing ZFS.  */

void
test_zfs (thread *t)
{
  dir_op_res res;
  dir_op_res res2;
  zfs_cap cap;
  int test = 0;
  string rmdir_name = {3, "dir"};
  sattr attr = {0755, 0, 0, (uint64_t) -1, (zfs_time) -1, (zfs_time) -1};

  if (0)
    {
      test_splay ();
      test_interval ();
    }

  if (strcmp (node_name, "orion") == 0)
    {
      node nod;
      char *str;
      int r;

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST %d\n", ++test);
      zfs_proc_null_client (t, NULL, nod);

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST %d\n", ++test);
      zfs_proc_root_client (t, NULL, nod);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/volume3/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/volume3/subdir");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      printf ("%d\n", zfs_mkdir (&res2, &res.file, &rmdir_name, &attr));

      message (2, stderr, "TEST %d\n", ++test);
      printf ("%d\n", zfs_rmdir (&res.file, &rmdir_name));

      message (2, stderr, "TEST %d\n", ++test);
      printf ("%d\n", r = zfs_open_by_fh (&cap, &res.file, O_RDONLY));

      if (r == ZFS_OK)
	{
	  message (2, stderr, "TEST %d\n", ++test);
	  printf ("%d\n", zfs_close (&cap));
	}
    }
}
