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

static void
print_mode (uint32_t mode)
{
  printf (" Mode: ");
  printf (mode & S_IRUSR ? "r" : "-");
  printf (mode & S_IWUSR ? "w" : "-");
  printf (mode & S_ISUID
	  ? (mode & S_IXUSR ? "s" : "S")
	  : (mode & S_IXUSR ? "x" : "-"));
  printf (mode & S_IRGRP ? "r" : "-");
  printf (mode & S_IWGRP ? "w" : "-");
  printf (mode & S_ISGID
	  ? (mode & S_IXGRP ? "s" : "S")
	  : (mode & S_IXGRP ? "x" : "-"));
  printf (mode & S_IROTH ? "r" : "-");
  printf (mode & S_IWOTH ? "w" : "-");
  printf (mode & S_ISVTX
	  ? (mode & S_IXOTH ? "t" : "T")
	  : (mode & S_IXOTH ? "x" : "-"));
  printf ("\n");
}

int main (int argc, char **argv)
{
  hfile_t hfile;
  int i;
  struct stat st;
  metadata meta;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: dump_metadata METADATA_FILE LOCAL_FILE...\n");
      return 1;
    }

  hfile = hfile_create (sizeof (metadata), offsetof (metadata, parent_dev),
			256, metadata_hash, metadata_eq,
			metadata_decode, metadata_encode, argv[1], NULL);
  hfile->fd = open (hfile->file_name, O_RDONLY);
  if (hfile->fd < 0)
    {
      perror (argv[1]);
      return 1;
    }
  if (!hfile_init (hfile, &st))
    {
      fprintf (stderr, "%s: Can't initialize hash file\n", argv[1]);
      return 1;
    }

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
	{
	  fprintf (stderr, "%s: Can't find in hash file\n", argv[i]);
	  continue;
	}

      printf ("\n%s:\n", argv[i]);

      printf (" Flags:");
      if (meta.flags & METADATA_COMPLETE)
	printf (" complete");
      if (meta.flags & METADATA_MODIFIED)
	printf (" modified");
      printf ("\n");

      printf (" Generation: %" PRIu32 "\n", meta.gen);
      printf (" Local version: %" PRIu64 "\n", meta.local_version);
      printf (" Master version: %" PRIu64 "\n", meta.master_version);
      printf (" UID: %" PRIu32 "\n", meta.uid);
      printf (" GID: %" PRIu32 "\n", meta.gid);
      print_mode (meta.mode);
      printf (" Master FH: [%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
	      ",%" PRIu32 "]\n", meta.master_fh.sid, meta.master_fh.vid,
	      meta.master_fh.dev, meta.master_fh.ino, meta.master_fh.gen);
    }

  return 0;
}
