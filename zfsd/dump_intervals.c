/* Dump intervals in the interval files.
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
   or download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "interval.h"

int main (int argc, char **argv)
{
  int fd, i;
  interval_tree tree;
  struct stat st;

  if (argc < 2)
    {
      fprintf (stderr, "Usage: dump_intervals INTERVAL_FILE...\n");
      return 1;
    }

  for (i = 1; i < argc; i++)
    {
      fd = open (argv[i], O_RDONLY);
      if (fd < 0)
	{
	  perror (argv[i]);
	  continue;
	}

      if (fstat (fd, &st) != 0)
	{
	  perror (argv[i]);
	  continue;
	}

      tree = interval_tree_create (1020, NULL);
      if (!interval_tree_read (tree, fd, st.st_size / sizeof (interval)))
	{
	  fprintf (stderr, "%s: Could not read intervals\n", argv[i]);
	  close (fd);
	  interval_tree_destroy (tree);
	  continue;
	}
      close (fd);

      printf ("%s:\n", argv[i]);
      print_interval_tree (stdout, tree);
      interval_tree_destroy (tree);
    }

  return 0;
}
