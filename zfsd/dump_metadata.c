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
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "hashfile.h"
#include "metadata.h"
#include "hardlink-list.h"
#include "journal.h"
#include "volume.h"

static void
print_modetype (uint32_t mode)
{
  printf (" Mode: ");
  switch (GET_MODETYPE_TYPE (mode))
    {
      default:
	printf ("X");
	break;

      case FT_BAD:
	printf ("x");
	break;

      case FT_REG:
	printf ("-");
	break;

      case FT_DIR:
	printf ("d");
	break;

      case FT_LNK:
	printf ("l");
	break;

      case FT_BLK:
	printf ("b");
	break;

      case FT_CHR:
	printf ("c");
	break;

      case FT_SOCK:
	printf ("s");
	break;

      case FT_FIFO:
	printf ("p");
	break;
    }

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
  int i;
  struct stat st;
  metadata meta;
  zfs_fh fh;
  volume vol;
  hardlink_list hl;
  journal_t journal;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: dump_metadata VOLUME_ROOT LOCAL_FILE...\n");
      return 1;
    }

  vol = (volume) xmalloc (sizeof (struct volume_def));
  xmkstring (&vol->local_path, argv[1]);
  zfsd_mutex_init (&vol->mutex);
  zfsd_mutex_lock (&vol->mutex);

  init_constants ();
  initialize_metadata_c ();
  initialize_hardlink_list_c ();
  initialize_journal_c ();

  if (!init_volume_metadata (vol))
    {
      fprintf (stderr, "%s: Could not initialize metadata\n",
	       vol->local_path.str);
      return 1;
    }

  hl = hardlink_list_create (32, NULL);
  journal = journal_create (32, NULL);

  for (i = 2; i < argc; i++)
    {
      if (lstat (argv[i], &st) != 0)
	{
	  perror (argv[i]);
	  continue;
	}

      fh.dev = meta.dev = st.st_dev;
      fh.ino = meta.ino = st.st_ino;
      if (!hfile_lookup (vol->metadata, &meta))
	{
	  fprintf (stderr, "%s: Can't find in hash file\n", argv[i]);
	  continue;
	}

      printf ("\n%s:\n", argv[i]);

      if (meta.slot_status != VALID_SLOT)
	continue;

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
      print_modetype (meta.modetype);
      printf (" Master FH: [%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
	      ",%" PRIu32 "]\n", meta.master_fh.sid, meta.master_fh.vid,
	      meta.master_fh.dev, meta.master_fh.ino, meta.master_fh.gen);

      read_hardlinks (vol, &fh, &meta, hl);
      if (hl->first)
	{
	  printf (" Hardlinks:\n");
	  print_hardlink_list (stdout, hl);
	  hardlink_list_empty (hl);
	}

      read_journal (vol, &fh, journal);
      if (journal->first)
	{
	  printf (" Journal:\n");
	  print_journal (stdout, journal);
	  journal_empty (journal);
	}
    }

  hardlink_list_destroy (hl);
  journal_destroy (journal);
  close_volume_metadata (vol);

  cleanup_journal_c ();
  cleanup_hardlink_list_c ();
  cleanup_metadata_c ();

  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_destroy (&vol->mutex);
  free (vol->local_path.str);
  free (vol);

  return 0;
}
