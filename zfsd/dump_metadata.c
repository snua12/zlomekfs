/* Dump metadata for local files.
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
#include "hashfile.h"
#include "metadata.h"

int main (int argc, char **argv)
{
  hfile_t hfile;
  int i;
  struct stat st;
  metadata meta;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: dump_metadata LIST_FILE LOCAL_FILE...\n");
      return 1;
    }

  hfile = hfile_create (sizeof (metadata), 256, metadata_hash, metadata_eq,
			metadata_decode, metadata_encode, argv[1], NULL);
  hfile->fd = open (hfile->file_name, O_RDONLY);
  if (hfile->fd < 0)
    return 1;
  if (!hfile_init (hfile, &st))
    return 1;

  for (i = 2; i < argc; i++)
    {
      if (lstat (argv[i], &st) != 0)
	{
	  perror (argv[i]);
	  continue;
	}

      meta.dev = st.st_dev;
      meta.ino = st.st_ino;
      if (!hfile_lookup (hfile, &meta))
	continue;

      printf ("%s:\n", argv[i]);

      printf ("  Flags:");
      if (meta.flags & METADATA_COMPLETE)
	printf (" complete");
      if (meta.flags & METADATA_MODIFIED)
	printf (" modified");
      printf ("\n");

      printf ("  Local version: %" PRIu64 "\n", meta.local_version);
      printf ("  Master version: %" PRIu64 "\n", meta.master_version);
    }

  return 0;
}
