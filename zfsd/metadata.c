/* Metadata management functions.
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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"
#include "metadata.h"
#include "constant.h"
#include "memory.h"
#include "interval.h"
#include "varray.h"
#include "fh.h"
#include "config.h"

/* Build path to file with interval tree of purpose PURPOSE for file handle FH
   on volume VOL, the depth of metadata directory tree is TREE_DEPTH.  */

static char *
build_metadata_path (volume vol, internal_fh fh, interval_tree_purpose purpose,
		     unsigned int tree_depth)
{
  char name[17];
  char tree[2 * MAX_METADATA_TREE_DEPTH + 1];
  char *path;
  varray v;
  unsigned int i;

#ifdef ENABLE_CHECKING
  if (tree_depth > MAX_METADATA_TREE_DEPTH)
    abort ();
#endif

  sprintf (name, "%08X%08X", fh->local_fh.dev, fh->local_fh.ino);
#ifdef ENABLE_CHECKING
  if (name[16] != 0)
    abort ();
#endif

  for (i = 0; i < tree_depth; i++)
    {
      tree[2 * i] = name[15 - i];
      tree[2 * i + 1] = '/';
    }
  tree[2 * tree_depth] = 0;

  varray_create (&v, sizeof (char *), 4);
  VARRAY_PUSH (v, vol->local_path, char *);
  switch (purpose)
    {
      case INTERVAL_TREE_UPDATED:
	VARRAY_PUSH (v, "/.zfs/updated/", char *);
	break;

      case INTERVAL_TREE_MODIFIED:
	VARRAY_PUSH (v, "/.zfs/modified/", char *);
	break;
    }
  VARRAY_PUSH (v, tree, char *);
  VARRAY_PUSH (v, name, char *);

  path = xstrconcat_varray (&v);
  varray_destroy (&v);

  return path;
}

/* Create a full path to file FILE with access rights MODE.
   Return true if path exists at the end of this function.  */

static bool
create_path_for_file (char *file, unsigned int mode)
{
  struct stat st;
  char *last;
  char *end;

  for (last = file; *last; last++)
    ;
  last--;

  /* Find the first existing directory.  */
  for (end = last;;)
    {
      for (; end != file && *end != '/'; end--)
	;
      if (end == file)
	return false;

      *end = 0;

      if (lstat (file, &st) == 0)
	{
	  if ((st.st_mode & S_IFMT) != S_IFDIR)
	    return false;

	  break;
	}
    }

  /* Create the path.  */
  for (;;)
    {
      *end = '/';

      if (mkdir (file, mode) != 0)
	return false;

      for (end++; end < last && *end; end++)
	;
      if (end >= last)
	return true;
    }

  return false;
}
