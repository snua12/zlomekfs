/* Directory operations.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "fh.h"
#include "dir.h"

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_extended_lookup (svc_fh *fh, svc_fh *dir, char *path)
{
  svc_fh tmp;
  char *s;
  int r;

  tmp = (*path == '/') ? root_fh : *dir;
  while (*path)
    {
      while (*path == '/')
	path++;

      s = path;
      while (*path != 0 && *path != '/')
	path++;
      if (*path == '/')
	*path++ = 0;

      r = zfs_lookup (&tmp, &tmp, s);
      if (r)
	return r;
    }

  *fh = tmp;
  return 0;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_lookup (svc_fh *fh, svc_fh *dir, const char *name)
{
  internal_fh idir;
  internal_fh ifh;

  /* Lookup the DIR.  */
  idir = (internal_fh) fh_lookup (dir);
  if (!idir)
    return ESTALE;

  /* Lookup the NAME in DIR.  */
  ifh = fh_lookup_name (idir, name);
  if (ifh && VIRTUAL_FH_P (ifh->client_fh) && ifh->vd->active)
    {
      *fh = ifh->client_fh;
      return 0;
    }
  
  if (!ifh)
    {
      if (VIRTUAL_FH_P (idir->client_fh))
	{
	}
    }






  return 0;
}

#if 0
int
zfs_readdir (svc_fh *dir, 
#endif
