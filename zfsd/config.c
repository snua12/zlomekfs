/* Configuration.
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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <sys/utsname.h>
#include "pthread.h"
#include "config.h"
#include "constant.h"
#include "log.h"
#include "memory.h"
#include "alloc-pool.h"
#include "semaphore.h"
#include "node.h"
#include "volume.h"
#include "metadata.h"
#include "user-group.h"
#include "thread.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "network.h"
#include "zfsd.h"

#ifdef BUFSIZ
#define LINE_SIZE BUFSIZ
#else
#define LINE_SIZE 2048
#endif

/* Data for config reader thread.  */
thread config_reader_data;

/* File used to communicate with kernel.  */
string kernel_file_name;

/* Directory with node configuration. */
string node_config;

/* File with private key.  */
static string private_key;

/* Element of list of requests for config reread.  */
typedef struct reread_config_request_def *reread_config_request;
struct reread_config_request_def
{
  /* Next element in the chain.  */
  reread_config_request next;

  /* Path relative to root of config volume.  */
  string relative_path;

  /* Node which the request came from.  */
  uint32_t from_sid;
};

/* First and last element of the chain of requests for rereading
   configuration.  */
static reread_config_request reread_config_first;
static reread_config_request reread_config_last;

/* Alloc pool for allocating nodes of reread config chain.  */
static alloc_pool reread_config_pool;

/* Mutex protecting the reread_config chain and alloc pool.  */
static pthread_mutex_t reread_config_mutex;

/* Process one line of configuration file.  Return the length of value.  */

static int
process_line (const char *file, const int line_num, char *line, char **key,
	      char **value)
{
  char *dest;
  enum automata_states {
    STATE_NORMAL,		/* outside quotes and not after backslash */
    STATE_QUOTED,		/* inside quotes and not after backslash  */
    STATE_BACKSLASH,		/* outside quotes and after backslash */
    STATE_QUOTED_BACKSLASH	/* inside quotes and after backslash */
  } state;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  if (*line == 0 || *line == '#' || *line == '\n')
    {
      /* There was no key nor value.  */
      *line = 0;
      *key = line;
      *value = line;
      return 0;
    }

  *key = line;
  /* Skip the key.  */
  while (*line != 0 && *line != '#' && *line != '\n'
	 && *line != ' ' && *line != '\t')
    line++;

  if (*line == 0 || *line == '#' || *line == '\n')
    {
      *line = 0;
      message (0, stderr, "%s:%d: Option '%s' has no value\n",
	       file, line_num, *key);
      return 0;
    }
  *line = 0;
  line++;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  *value = line;
  dest = line;

  /* Finite automata.  */
  state = STATE_NORMAL;
  while (*line != 0)
    {
      switch (state)
	{
	  case STATE_NORMAL:
	    switch (*line)
	      {
		case '"':
		  line++;
		  state = STATE_QUOTED;
		  break;

		case '\\':
		  line++;
		  state = STATE_BACKSLASH;
		  break;

		case ' ':
		case '\t':
		case '#':
		case '\n':
		  *line = 0;
		  break;

		default:
		  *dest++ = *line++;
		  break;
	      }
	    break;

	  case STATE_QUOTED:
	    switch (*line)
	      {
		case '"':
		  line++;
		  state = STATE_NORMAL;
		  break;

		case '\\':
		  line++;
		  state = STATE_QUOTED_BACKSLASH;
		  break;

		case '\n':
		case 0:
		  *line = 0;
		  break;

		default:
		  *dest++ = *line++;
		  break;
	      }
	    break;

	  case STATE_BACKSLASH:
	    *dest++ = *line++;
	    state = STATE_NORMAL;
	    break;

	  case STATE_QUOTED_BACKSLASH:
	    *dest++ = *line++;
	    state = STATE_QUOTED;
	    break;
	}
    }

  /* If there was '\\' on the end of line, add it to the end of string. */
  if (state == STATE_BACKSLASH || state == STATE_QUOTED_BACKSLASH)
    *dest++ = '\\';
  *dest = 0;

  if (*value == dest)
    {
      message (0, stderr, "%s:%d: Option '%s' has no value\n",
	       file, line_num, *key);
      return 0;
    }
  return dest - *value;
}

/* Split the line by ':', trim the resulting parts, fill up to N parts
   to PARTS and return the total number of parts.  */

static int
split_and_trim (char *line, int n, string *parts)
{
  int i;
  char *start, *colon;

  i = 0;
  while (1)
    {
      /* Skip white spaces.  */
      while (*line == ' ' || *line == '\t')
	line++;

      /* Remember the beginning of a part. */
      start = line;
      if (i < n)
	parts[i].str = start;

      /* Find the end of a part.  */
      while (*line != 0 && *line != '\n' && *line != ':')
	line++;
      colon = line;

      if (i < n)
	{
	  if (line > start)
	    {
	      /* Delete white spaces at the end of a part.  */
	      line--;
	      while (line >= start && (*line == ' ' || *line == '\t'))
		{
		  *line = 0;
		  line--;
		}
	      line++;
	    }
	  parts[i].len = line - start;
	}

      i++;

      if (*colon == ':')
	{
	  *colon = 0;
	  line = colon + 1;
	}
      else
	{
	  /* We are at the end of line.  */
	  *colon = 0;
	  break;
	}
    }

  return i;
}

/* Get the name of local node.  */
void
set_node_name (void)
{
  struct utsname un;

  if (uname (&un) != 0)
    return;

  set_str (&node_host_name, un.nodename);
  message (1, stderr, "Autodetected node name: '%s'\n", un.nodename);
}

/* Set default node UID to UID of user NAME.  Return true on success.  */

static bool
set_default_uid (char *name)
{
  struct passwd *pwd;

  pwd = getpwnam (name);
  if (!pwd)
    return false;

  default_node_uid = pwd->pw_uid;
  return true;
}

/* Set default node GID to GID of group NAME.  Return true on success.  */

static bool
set_default_gid (char *name)
{
  struct group *grp;

  grp = getgrnam (name);
  if (!grp)
    return false;

  default_node_gid = grp->gr_gid;
  return true;
}

/* Set default local user/group.  */

void
set_default_uid_gid (void)
{
  set_default_uid ("nobody");
  if (!set_default_gid ("nogroup"))
    set_default_gid ("nobody");
}

static bool
read_private_key (string *filename)
{

  free (filename->str);
  return true;
}

static bool
read_local_cluster_config (string *path)
{
  char *file;
  FILE *f;
  int line_num;
  string parts[3];
  char line[LINE_SIZE + 1];

  if (path->str == 0)
    {
      message (0, stderr,
	       "The directory with configuration of local node is not specified"
	       "in configuration file.\n");
      return false;
    }
  message (2, stderr, "Reading configuration of local node\n");

  /* Read ID of local node.  */
  file = xstrconcat (2, path->str, "/this_node");
  f = fopen (file, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", file, strerror (errno));
      free (file);
      return false;
    }
  if (!fgets (line, sizeof (line), f))
    {
      message (0, stderr, "%s: Could not read a line\n", file);
      free (file);
      fclose (f);
      return false;
    }
  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &this_node_id) != 1)
	{
	  message (0, stderr, "%s: Could not read node ID\n", file);
	  free (file);
	  fclose (f);
	  return false;
	}
      if (this_node_id == 0 || this_node_id == (uint32_t) -1)
	{
	  message (0, stderr, "%s: Node ID must not be 0 or %" PRIu32, file,
		   (uint32_t) -1);
	  free (file);
	  fclose (f);
	  return false;
	}
      if (parts[1].len == 0)
	{
	  message (0, stderr, "%s: Node name must not be empty\n", file);
	  free (file);
	  fclose (f);
	  return false;
	}
      xstringdup (&node_name, &parts[1]);
    }
  else
    {
      message (0, stderr, "%s:1: Wrong format of line\n", file);
      free (file);
      fclose (f);
      return false;
    }
  free (file);
  fclose (f);

  /* Read local info about volumes.  */
  file = xstrconcat (2, path->str, "/volume_info");
  f = fopen (file, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", file, strerror (errno));
      free (file);
      return false;
    }
  else
    {
      line_num = 0;
      while (!feof (f))
	{
	  if (!fgets (line, sizeof (line), f))
	    break;

	  line_num++;
	  if (split_and_trim (line, 3, parts) == 3)
	    {
	      volume vol;
	      uint32_t id;
	      uint64_t size_limit;

	      /* 0 ... ID
		 1 ... local path
		 2 ... size limit */
	      if (sscanf (parts[0].str, "%" PRIu32, &id) != 1
		  || sscanf (parts[2].str, "%" PRIu64, &size_limit) != 1)
		{
		  message (0, stderr, "%s:%d: Wrong format of line\n", file,
			   line_num);
		}
	      else if (id == 0 || id == (uint32_t) -1)
		{
		  message (0, stderr,
			   "%s:%d: Volume ID must not be 0 or %" PRIu32 "\n",
			   file, line_num, (uint32_t) -1);
		}
	      else if (parts[1].str[0] != '/')
		{
		  message (0, stderr,
			   "%s:%d: Local path must be an absolute path\n",
			   file, line_num);
		}
	      else
		{
		  zfsd_mutex_lock (&fh_mutex);
		  zfsd_mutex_lock (&volume_mutex);
		  vol = volume_create (id);
		  if (volume_set_local_info (vol, &parts[1], size_limit))
		    zfsd_mutex_unlock (&vol->mutex);
		  else
		    {
		      message (0, stderr, "Could not set local information"
			       " about volume with ID = %" PRIu32 "\n", id);
		      volume_delete (vol);
		    }
		  zfsd_mutex_unlock (&volume_mutex);
		  zfsd_mutex_unlock (&fh_mutex);
		}
	    }
	  else
	    {
	      message (0, stderr, "%s:%d: Wrong format of line\n", file,
		       line_num);
	    }
	}
      fclose (f);
    }

  free (file);
  return true;
}

/* Initialize data structures which are needed for reading configuration.  */

static bool
init_config (void)
{
  volume vol;
  node nod;

  zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_lookup_nolock (VOLUME_ID_CONFIG);
  if (!vol)
    {
      message (0, stderr, "Config volume (ID == %" PRIu32 " does not exist.\n",
	       VOLUME_ID_CONFIG);
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&vd_mutex);
      goto out;
    }

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (this_node_id, &node_name, &node_host_name);
  zfsd_mutex_unlock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);

  volume_set_common_info_wrapper (vol, "config", "/config", nod);

  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&volume_mutex);
  zfsd_mutex_unlock (&vd_mutex);
  return true;

out:
  destroy_all_volumes ();
  return false;
}

/* Read file FH by lines and call function PROCESS for each line.  */

static bool
process_file_by_lines (zfs_fh *fh, char *file_name,
		       int (*process) (char *, char *, unsigned int, void *),
		       void *data)
{
  char buf[ZFS_MAXDATA];
  unsigned int index, i, line_num;
  uint32_t count;
  uint64_t offset;
  zfs_cap cap;
  int32_t r;

  r = zfs_open (&cap, fh, O_RDONLY);
  if (r != ZFS_OK)
    return false;

  line_num = 1;
  index = 0;
  offset = 0;
  for (;;)
    {
      r = zfs_read (&count, buf + index, &cap, offset, ZFS_MAXDATA - index,
		    true);
      if (r != ZFS_OK)
	return false;

      if (count == 0)
	break;

      offset += count;
      count += index;
      for (index = 0; index < count; index = i + 1)
	{
	  for (i = index; i < count; i++)
	    if (buf[i] == '\n')
	      {
		buf[i] = 0;
		if ((*process) (buf + index, file_name, line_num, data) != 0)
		  goto finish;
		line_num++;
		break;
	      }
	  if (i == count)
	    break;
	}

      if (index == 0 && i == ZFS_MAXDATA)
	{
	  message (0, stderr, "%s:%u: Line too long\n", file_name, line_num);
	  goto out;
	}
      if (index > 0)
	{
	  memmove (buf, buf + index, count - index);
	  index = count - index;
	}
      else
	{
	  /* The read block does not contain new line.  */
	  index = count;
	}
    }

finish:
  r = zfs_close (&cap);
  if (r != ZFS_OK)
    return false;

  return true;

out:
  zfs_close (&cap);
  return false;
}

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_node (char *line, char *file_name, unsigned int line_num,
		   ATTRIBUTE_UNUSED void *data)
{
  string parts[3];
  uint32_t sid;
  node nod;

  if (split_and_trim (line, 3, parts) == 3)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &sid) != 1)
	{
	  message (0, stderr, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (sid == 0 || sid == (uint32_t) -1)
	{
	  message (0, stderr, "%s:%u: Node ID must not be 0 or %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: Node name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[2].len == 0)
	{
	  message (0, stderr, "%s:%u: Host name must not be empty\n",
		   file_name, line_num);
	}
      else
	{
	  nod = try_create_node (sid, &parts[1], &parts[2]);
	  if (nod)
	    zfsd_mutex_unlock (&nod->mutex);
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of nodes from CONFIG_DIR/node_list.  */

static bool
read_node_list (zfs_fh *config_dir)
{
  dir_op_res node_list_res;
  int32_t r;

  r = zfs_extended_lookup (&node_list_res, config_dir, "node_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&node_list_res.file, "config/node_list",
				process_line_node, NULL);
}

/* Data for process_line_volume_hierarchy.  */
typedef struct volume_hierarchy_data_def
{
  varray hierarchy;
  uint32_t vid;
  uint32_t depth;
  string *name;
  string *mountpoint;
} volume_hierarchy_data;

/* Process line LINE number LINE_NUM of volume hierarchy file FILE_NAME
   and update hierarchy DATA.  */

static int
process_line_volume_hierarchy (char *line, ATTRIBUTE_UNUSED char *file_name,
			       ATTRIBUTE_UNUSED unsigned int line_num,
			       void *data)
{
  volume_hierarchy_data *d = (volume_hierarchy_data *) data;
  char *name;
  uint32_t i;
  volume vol;
  node nod;
  string str;
  void **slot;

  for (i = 0; line[i] == ' '; i++)
    ;
  if (line[i] == 0)
    return 0;

  if (d->depth == 0)
    {
      /* Free superfluous records.  */
      while (VARRAY_USED (d->hierarchy) > i)
	{
	  name = VARRAY_TOP (d->hierarchy, char *);
	  if (name)
	    free (name);
	  VARRAY_POP (d->hierarchy);
	}

      if (strncmp (line + i, node_name.str, node_name.len + 1) == 0)
	{
	  char *master_name = NULL;

	  /* We are processing the local node.  */

	  d->depth = i + 1;
	  while (VARRAY_USED (d->hierarchy) > 0)
	    {
	      master_name = VARRAY_TOP (d->hierarchy, char *);
	      if (master_name)
		break;
	      VARRAY_POP (d->hierarchy);
	    }

	  if (master_name)
	    {
	      str.str = master_name;
	      str.len = strlen (master_name);
	      nod = node_lookup_name (&str);
	      if (nod)
		zfsd_mutex_unlock (&nod->mutex);
	    }
	  else
	    nod = this_node;

	  if (nod)
	    {
	      zfsd_mutex_lock (&vd_mutex);
	      zfsd_mutex_lock (&volume_mutex);
	      vol = volume_lookup_nolock (d->vid);
	      if (!vol)
		vol = volume_create (d->vid);
	      else
		{
		  if (vol->slaves)
		    htab_empty (vol->slaves);
		}
	      volume_set_common_info (vol, d->name, d->mountpoint, nod);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&volume_mutex);
	      zfsd_mutex_unlock (&vd_mutex);

	      /* Continue reading the file because we need to read the list
		 of nodes whose master is local node.  */
	      if (vol->slaves && !vol->marked)
		return 0;
	    }

	  return 1;
	}

      /* Add missing empty records.  */
      while (VARRAY_USED (d->hierarchy) < i)
	VARRAY_PUSH (d->hierarchy, NULL, char *);

      name = xstrdup (line + i);
      VARRAY_PUSH (d->hierarchy, name, char *);
    }
  else
    {
      /* We have created/updated the volume, read the list of nodes whose
	 master is local node.  */

      if (i < d->depth)
	{
	  /* The subtree of local node has been processed, stop reading the
	     file.  */
	  return 1;
	}

      /* Free superfluous records.  */
      while (VARRAY_USED (d->hierarchy) > i)
	{
	  name = VARRAY_TOP (d->hierarchy, char *);
	  if (name)
	    free (name);
	  VARRAY_POP (d->hierarchy);
	}

      /* Push missing empty records.  */
      while (VARRAY_USED (d->hierarchy) < i)
	VARRAY_PUSH (d->hierarchy, NULL, char *);

      /* Do not add local node to list of slaves.  */
      if (strcmp (line + i, this_node->name.str) == 0)
	return 0;

      name = xstrdup (line + i);
      VARRAY_PUSH (d->hierarchy, name, char *);

      /* All records in hierarchy upto current node must be NULL so that
	 local node would be master of current node.  */
      for (i = d->depth; i < VARRAY_USED (d->hierarchy) - 1; i++)
	if (VARRAY_ACCESS (d->hierarchy, i, char *) != NULL)
	  {
	    /* The current node is not direct descendant of local node
	       so continue reading the file.  */
	    return 0;
	  }

      vol = volume_lookup (d->vid);
      if (!vol)
	{
	  /* Volume was destroyed meanwhile.  */
	  return 1;
	}
#ifdef ENABLE_CHECKING
      if (!vol->slaves)
	abort ();
#endif

      str.str = line + i;
      str.len = strlen (line + i);
      nod = node_lookup_name (&str);
      if (!nod)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  return 0;
	}
      if (vol->master == nod)
	{
	  zfsd_mutex_unlock (&nod->mutex);
	  zfsd_mutex_unlock (&vol->mutex);
	  return 0;
	}

      slot = htab_find_slot_with_hash (vol->slaves, nod, NODE_HASH_NAME (nod),
				       INSERT);
      *slot = nod;
      zfsd_mutex_unlock (&nod->mutex);
      zfsd_mutex_unlock (&vol->mutex);
    }

  return 0;
}

/* Read appropriate file in VOLUME_INFO_DIR and process info about volume VID
   with name NAME and volume mountpoint MOUNTPOINT.  */

static void
read_volume_hierarchy (zfs_fh *volume_hierarchy_dir, uint32_t vid,
		       string *name, string *mountpoint)
{
  volume_hierarchy_data data;
  dir_op_res file_res;
  char *file_name, *master_name;
  int32_t r;

  r = zfs_extended_lookup (&file_res, volume_hierarchy_dir, name->str);
  if (r != ZFS_OK)
    return;

  varray_create (&data.hierarchy, sizeof (char *), 4);
  data.vid = vid;
  data.depth = 0;
  data.name = name;
  data.mountpoint = mountpoint;

  file_name = xstrconcat (2, "config/volume/", name->str);
  process_file_by_lines (&file_res.file, file_name,
			 process_line_volume_hierarchy, &data);
  free (file_name);

  /* Set the common volume info for volumes which were not listed in volume
     hierarchy.  */
  if (VARRAY_USED (data.hierarchy) > 0)
    {
      unsigned int i;
      string str;
      volume vol;
      node nod;

      for (i = 0; i < VARRAY_USED (data.hierarchy); i++)
	{
	  master_name = VARRAY_ACCESS (data.hierarchy, i, char *);
	  if (master_name)
	    break;
	}

      if (master_name)
	{
	  str.str = master_name;
	  str.len = strlen (master_name);
	  nod = node_lookup_name (&str);
	  if (!nod)
	    goto out;
	  zfsd_mutex_unlock (&nod->mutex);

	  zfsd_mutex_lock (&vd_mutex);
	  zfsd_mutex_lock (&volume_mutex);
	  vol = volume_lookup_nolock (vid);
	  if (!vol)
	    vol = volume_create (vid);
	  else
	    {
	      if (!vol->marked)
		{
		  zfsd_mutex_unlock (&vol->mutex);
		  zfsd_mutex_unlock (&volume_mutex);
		  zfsd_mutex_unlock (&vd_mutex);
		  goto out;
		}
	      if (vol->slaves)
		htab_empty (vol->slaves);
	    }
	  volume_set_common_info (vol, name, mountpoint, nod);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
    }

out:
  while (VARRAY_USED (data.hierarchy) > 0)
    {
      master_name = VARRAY_TOP (data.hierarchy, char *);
      if (master_name)
	free (master_name);
      VARRAY_POP (data.hierarchy);
    }
  varray_destroy (&data.hierarchy);
}

/* Saved information about config volume because we need to update it after
   information about every volume was read.  */

static uint32_t saved_vid;
static string saved_name;
static string saved_mountpoint;

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_volume (char *line, char *file_name, unsigned int line_num,
		     void *data)
{
  zfs_fh *volume_hierarchy_dir = (zfs_fh *) data;
  string parts[3];
  uint32_t vid;

  if (split_and_trim (line, 3, parts) == 3)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &vid) != 1)
	{
	  message (0, stderr, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (vid == 0 || vid == (uint32_t) -1)
	{
	  message (0, stderr, "%s:%u: Volume ID must not be 0 or %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: Volume name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[2].str[0] != '/')
	{
	  message (0, stderr,
		   "%s:%d: Volume mountpoint must be an absolute path\n",
		   file_name, line_num);
	}
      else if (vid == VOLUME_ID_CONFIG && saved_vid == 0)
	{
	  volume vol;

	  saved_vid = vid;
	  if (strcmp (parts[2].str, "/config") != 0)
	    {
	      message (0, stderr,
		       "%s:%d: Mountpoint of config volume must be '/config'\n",
		       file_name, line_num);
	      saved_mountpoint.str = NULL;
	    }

	  xstringdup (&saved_name, &parts[1]);
	  xstringdup (&saved_mountpoint, &parts[2]);

	  zfsd_mutex_lock (&vd_mutex);
	  zfsd_mutex_lock (&volume_mutex);
	  vol = volume_lookup_nolock (vid);
#ifdef ENABLE_CHECKING
	  if (!vol)
	    abort ();
	  else
#endif
	    {
	      if (vol->slaves)
		htab_empty (vol->slaves);
	    }
	  volume_set_common_info (vol, &parts[1], &parts[2], this_node);
	  vol->marked = true;
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      else
	{
	  read_volume_hierarchy (volume_hierarchy_dir, vid, &parts[1],
				 &parts[2]);
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of volumes from CONFIG_DIR/volume_list.  */

static bool
read_volume_list (zfs_fh *config_dir)
{
  dir_op_res volume_list_res;
  dir_op_res volume_hierarchy_res;
  volume vol;
  int32_t r;

  r = zfs_extended_lookup (&volume_list_res, config_dir, "volume_list");
  if (r != ZFS_OK)
    return false;

  r = zfs_extended_lookup (&volume_hierarchy_res, config_dir, "volume");
  if (r != ZFS_OK)
    return false;

  saved_vid = 0;
  if (!process_file_by_lines (&volume_list_res.file, "config/volume_list",
			      process_line_volume,
			      &volume_hierarchy_res.file))
    return false;

  if (saved_vid == VOLUME_ID_CONFIG)
    {
      if (saved_mountpoint.str == NULL)
	return false;

      read_volume_hierarchy (&volume_hierarchy_res.file, saved_vid,
			     &saved_name, &saved_mountpoint);
      free (saved_name.str);
      free (saved_mountpoint.str);

      vol = volume_lookup (saved_vid);
      if (!vol)
	goto no_config;
      if (vol->marked)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  goto no_config;
	}
      zfsd_mutex_unlock (&vol->mutex);
    }
  else
    {
no_config:
      message (0, stderr,
	       "config/volume_list: Config volume does not exist\n");
      return false;
    }

  return true;
}

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_user (char *line, char *file_name, unsigned int line_num,
		   ATTRIBUTE_UNUSED void *data)
{
  string parts[2];
  uint32_t id;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &id) != 1)
	{
	  message (0, stderr, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (id == (uint32_t) -1)
	{
	  message (0, stderr, "%s:%u: User ID must not be %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: User name must not be empty\n",
		   file_name, line_num);
	}
      else
	{
	  zfsd_mutex_lock (&users_groups_mutex);
	  user_create (id, &parts[1]);
	  zfsd_mutex_unlock (&users_groups_mutex);
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of users from CONFIG_DIR/user_list.  */

static bool
read_user_list (zfs_fh *config_dir)
{
  dir_op_res user_list_res;
  int32_t r;

  r = zfs_extended_lookup (&user_list_res, config_dir, "user_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&user_list_res.file, "config/user_list",
				process_line_user, NULL);
}

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_group (char *line, char *file_name, unsigned int line_num,
		    ATTRIBUTE_UNUSED void *data)
{
  string parts[2];
  uint32_t id;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &id) != 1)
	{
	  message (0, stderr, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (id == (uint32_t) -1)
	{
	  message (0, stderr, "%s:%u: Group ID must not be %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: Group name must not be empty\n",
		   file_name, line_num);
	}
      else
	{
	  zfsd_mutex_lock (&users_groups_mutex);
	  group_create (id, &parts[1]);
	  zfsd_mutex_unlock (&users_groups_mutex);
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of groups from CONFIG_DIR/group_list.  */

static bool
read_group_list (zfs_fh *config_dir)
{
  dir_op_res group_list_res;
  int32_t r;

  r = zfs_extended_lookup (&group_list_res, config_dir, "group_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&group_list_res.file, "config/group_list",
				process_line_group, NULL);
}

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_user_mapping (char *line, char *file_name, unsigned int line_num,
			   void *data)
{
  uint32_t sid = *(uint32_t *) data;
  string parts[2];
  node nod;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (parts[0].len == 0)
	{
	  message (0, stderr, "%s:%u: ZFS user name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: Node user name must not be empty\n",
		   file_name, line_num);
	}
      else
	{
	  if (sid > 0)
	    {
	      nod = node_lookup (sid);
#ifdef ENABLE_CHECKING
	      if (!nod)
		abort ();
#endif
	      zfsd_mutex_lock (&users_groups_mutex);
	      user_mapping_create (&parts[0], &parts[1], nod);
	      zfsd_mutex_unlock (&users_groups_mutex);
	      zfsd_mutex_unlock (&nod->mutex);
	    }
	  else
	    {
	      zfsd_mutex_lock (&users_groups_mutex);
	      user_mapping_create (&parts[0], &parts[1], NULL);
	      zfsd_mutex_unlock (&users_groups_mutex);
	    }
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of user mapping.  If SID == 0 read the default user mapping
   from CONFIG_DIR/user/default else read the special mapping for node SID.  */

static bool
read_user_mapping (zfs_fh *user_dir, uint32_t sid)
{
  dir_op_res user_mapping_res;
  int32_t r;
  string node_name;
  char *file_name;
  bool ret;

  if (sid == 0)
    {
      node_name.str = "default";
      node_name.len = strlen ("default");
    }
  else
    {
      node nod;

      nod = node_lookup (sid);
      if (!nod)
	return false;

      xstringdup (&node_name, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);
    }

  r = zfs_extended_lookup (&user_mapping_res, user_dir, node_name.str);
  if (r != ZFS_OK)
    {
      if (sid != 0)
	free (node_name.str);
      return true;
    }

  file_name = xstrconcat (2, "config/user/", node_name.str);
  ret = process_file_by_lines (&user_mapping_res.file, file_name,
			       process_line_user_mapping, &sid);
  free (file_name);
  if (sid != 0)
    free (node_name.str);
  return ret;
}

/* Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_group_mapping (char *line, char *file_name, unsigned int line_num,
			    void *data)
{
  uint32_t sid = *(uint32_t *) data;
  string parts[2];
  node nod;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (parts[0].len == 0)
	{
	  message (0, stderr, "%s:%u: ZFS group name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[1].len == 0)
	{
	  message (0, stderr, "%s:%u: Node group name must not be empty\n",
		   file_name, line_num);
	}
      else
	{
	  if (sid > 0)
	    {
	      nod = node_lookup (sid);
#ifdef ENABLE_CHECKING
	      if (!nod)
		abort ();
#endif
	      zfsd_mutex_lock (&users_groups_mutex);
	      group_mapping_create (&parts[0], &parts[1], nod);
	      zfsd_mutex_unlock (&users_groups_mutex);
	      zfsd_mutex_unlock (&nod->mutex);
	    }
	  else
	    {
	      zfsd_mutex_lock (&users_groups_mutex);
	      group_mapping_create (&parts[0], &parts[1], NULL);
	      zfsd_mutex_unlock (&users_groups_mutex);
	    }
	}
    }
  else
    {
      message (0, stderr, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/* Read list of group mapping.  If SID == 0 read the default group mapping
   from CONFIG_DIR/group/default else read the special mapping for node SID.  */

static bool
read_group_mapping (zfs_fh *group_dir, uint32_t sid)
{
  dir_op_res group_mapping_res;
  int32_t r;
  string node_name;
  char *file_name;
  bool ret;

  if (sid == 0)
    {
      node_name.str = "default";
      node_name.len = strlen ("default");
    }
  else
    {
      node nod;

      nod = node_lookup (sid);
      if (!nod)
	return false;

      xstringdup (&node_name, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);
    }

  r = zfs_extended_lookup (&group_mapping_res, group_dir, node_name.str);
  if (r != ZFS_OK)
    {
      if (sid != 0)
	free (node_name.str);
      return true;
    }

  file_name = xstrconcat (2, "config/group/", node_name.str);
  ret = process_file_by_lines (&group_mapping_res.file, file_name,
			       process_line_group_mapping, &sid);
  free (file_name);
  if (sid != 0)
    free (node_name.str);
  return ret;
}

/* Invalidate configuration.  */

static void
invalidate_config (void)
{
  mark_all_nodes ();
  mark_all_volumes ();
  mark_all_users ();
  mark_all_groups ();
  mark_user_mapping (NULL);
  mark_group_mapping (NULL);
  if (this_node)
    {
      zfsd_mutex_lock (&this_node->mutex);
      mark_user_mapping (this_node);
      mark_group_mapping (this_node);
      zfsd_mutex_unlock (&this_node->mutex);
    }
}

/* Verify configuration, fix what can be fixed. Return false if there remains
   something which can't be fixed.  */

static bool
fix_config (void)
{
  if (this_node == NULL || this_node->marked)
    return false;

  destroy_invalid_volumes ();
  destroy_invalid_nodes ();

  destroy_invalid_user_mapping (NULL);
  destroy_invalid_group_mapping (NULL);

  zfsd_mutex_lock (&this_node->mutex);
  destroy_invalid_user_mapping (this_node);
  destroy_invalid_group_mapping (this_node);
  zfsd_mutex_unlock (&this_node->mutex);

  destroy_invalid_users ();
  destroy_invalid_groups ();

  return true;
}

/* Reread list of nodes.  */

static bool
reread_node_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    return false;

  mark_all_nodes ();

  if (!read_node_list (&config_dir_res.file))
    return false;

  if (this_node == NULL || this_node->marked)
    return false;

  destroy_invalid_volumes ();
  destroy_invalid_nodes ();

  return true;
}

/* Reread list of volumes.  */

static bool
reread_volume_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    return false;

  mark_all_volumes ();

  if (!read_volume_list (&config_dir_res.file))
    return false;

  destroy_invalid_volumes ();

  return true;
}

/* Reread list of users.  */

static bool
reread_user_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    return false;

  mark_all_users ();

  if (!read_user_list (&config_dir_res.file))
    return false;

  if (this_node)
    {
      zfsd_mutex_lock (&this_node->mutex);
      destroy_invalid_user_mapping (this_node);
      zfsd_mutex_unlock (&this_node->mutex);
    }
  destroy_invalid_user_mapping (NULL);
  destroy_invalid_users ();

  return true;
}

/* Reread list of groups.  */

static bool
reread_group_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    return false;

  mark_all_groups ();

  if (!read_group_list (&config_dir_res.file))
    return false;

  if (this_node)
    {
      zfsd_mutex_lock (&this_node->mutex);
      destroy_invalid_group_mapping (this_node);
      zfsd_mutex_unlock (&this_node->mutex);
    }
  destroy_invalid_group_mapping (NULL);
  destroy_invalid_groups ();

  return true;
}

/* Reread volume hierarchy for volume VOL.  */

static void
reread_volume_hierarchy (volume vol)
{
  dir_op_res volume_hierarchy_dir_res;
  int32_t r;
  uint32_t vid;
  string name;
  string mountpoint;

  vid = vol->id;
  xstringdup (&name, &vol->name);
  xstringdup (&mountpoint, &vol->mountpoint);
  vol->marked = true;
  zfsd_mutex_unlock (&vol->mutex);

  r = zfs_extended_lookup (&volume_hierarchy_dir_res, &root_fh,
			   "config/volume");
  if (r != ZFS_OK)
    {
      free (name.str);
      free (mountpoint.str);
      destroy_invalid_volume (vid);
      return;
    }

  read_volume_hierarchy (&volume_hierarchy_dir_res.file, vid, &name,
			 &mountpoint);

  destroy_invalid_volume (vid);
}

/* Reread user mapping for node SID.  */

static bool
reread_user_mapping (uint32_t sid)
{
  dir_op_res user_dir_res;
  int32_t r;
  node nod;

  r = zfs_extended_lookup (&user_dir_res, &root_fh, "config/user");
  if (r != ZFS_OK)
    return true;

  if (sid == 0)
    nod = NULL;
  else if (sid == this_node->id)
    nod = this_node;
  else
    return true;

  if (nod)
    {
      zfsd_mutex_lock (&nod->mutex);
      mark_user_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    mark_user_mapping (nod);

  if (!read_user_mapping (&user_dir_res.file, sid))
    return false;

  if (nod)
    {
      zfsd_mutex_lock (&nod->mutex);
      destroy_invalid_user_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    destroy_invalid_user_mapping (nod);

  return true;
}

/* Reread group mapping for node SID.  */

static bool
reread_group_mapping (uint32_t sid)
{
  dir_op_res group_dir_res;
  int32_t r;
  node nod;

  r = zfs_extended_lookup (&group_dir_res, &root_fh, "config/group");
  if (r != ZFS_OK)
    return true;

  if (sid == 0)
    nod = NULL;
  else if (sid == this_node->id)
    nod = this_node;
  else
    return true;

  if (nod)
    {
      zfsd_mutex_lock (&nod->mutex);
      mark_group_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    mark_group_mapping (nod);

  if (!read_group_mapping (&group_dir_res.file, sid))
    return false;

  if (nod)
    {
      zfsd_mutex_lock (&nod->mutex);
      destroy_invalid_group_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    destroy_invalid_group_mapping (nod);

  return true;
}

/* Reread configuration file RELATIVE_PATH.  */ 

static bool
reread_config_file (string *relative_path)
{
  string name;
  char *str = relative_path->str;

  if (strncmp (str, "/config/", 8) != 0)
    goto out_true;

  str += 8;
  if (strncmp (str, "node_list", 10) == 0)
    {
      if (!reread_node_list ())
	goto out;
    }
  else if (strncmp (str, "volume", 6) == 0)
    {
      str += 6;
      if (*str == '/')
	{
	  volume vol;

	  str++;
	  name.str = str;
	  name.len = relative_path->len - (str - relative_path->str);

	  vol = volume_lookup_name (&name);
	  if (vol)
	    reread_volume_hierarchy (vol);
	}
      else if (strncmp (str, "_list", 6) == 0)
	{
	  if (!reread_volume_list ())
	    goto out;
	}
    }
  else if (strncmp (str, "user", 4) == 0)
    {
      str += 4;
      if (*str == '/')
	{
	  str++;
	  if (strncmp (str, "default", 8) == 0)
	    {
	      if (!reread_user_mapping (0))
		goto out;
	    }
	  else if (strcmp (str, this_node->name.str) == 0)
	    {
	      if (!reread_user_mapping (this_node->id))
		goto out;
	    }
	}
      else if (strncmp (str, "_list", 6) == 0)
	{
	  if (!reread_user_list ())
	    goto out;
	}
    }
  else if (strncmp (str, "group", 5) == 0)
    {
      str += 5;
      if (*str == '/')
	{
	  str++;
	  if (strncmp (str, "default", 8) == 0)
	    {
	      if (!reread_group_mapping (0))
		goto out;
	    }
	  else if (strcmp (str, this_node->name.str) == 0)
	    {
	      if (!reread_group_mapping (this_node->id))
		goto out;
	    }
	}
      else if (strncmp (str, "_list", 6) == 0)
	{
	  if (!reread_group_list ())
	    goto out;
	}
    }

out_true:
  free (relative_path->str);
  return true;

out:
  free (relative_path->str);
  return false;
}

/* Add request to reread config file RELATIVE_PATH to queue.
   The request came from node FROM_SID.  */

void
add_reread_config_request (string *relative_path, uint32_t from_sid)
{
  reread_config_request node;

  if (get_thread_state (&config_reader_data) != THREAD_IDLE)
    return;

  zfsd_mutex_lock (&reread_config_mutex);
  node = (reread_config_request) pool_alloc (reread_config_pool);
  node->next = NULL;
  node->relative_path = *relative_path;
  node->from_sid = from_sid;

  if (reread_config_last)
    reread_config_last->next = node;
  else
    reread_config_first = node;
  reread_config_last = node;

  zfsd_mutex_unlock (&reread_config_mutex);
}

/* Get a request to reread config from queue and store the relative path of
   the file to be reread to RELATIVE_PATH and the node ID which the request came
   from to FROM_SID.  */

static bool
get_reread_config_request (string *relative_path, uint32_t *from_sid)
{
  zfsd_mutex_lock (&reread_config_mutex);
  if (reread_config_first == NULL)
    {
      zfsd_mutex_unlock (&reread_config_mutex);
      return false;
    }

  *relative_path = reread_config_first->relative_path;
  *from_sid = reread_config_first->from_sid;

  reread_config_first = reread_config_first->next;
  if (!reread_config_first)
    reread_config_last = NULL;

  zfsd_mutex_unlock (&reread_config_mutex);
  return true;
}

/* Has the config reader already terminated?  */
static volatile bool reading_cluster_config;

/* Thread for reading a configuration.  */

static void *
config_reader (void *data)
{
  thread *t = (thread *) data;
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  dir_op_res config_dir_res;
  dir_op_res user_dir_res;
  dir_op_res group_dir_res;
  int32_t r;
  volume vol;
  varray v;

  thread_disable_signals ();
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Config reader");
  set_lock_info (li);

  invalidate_config ();

  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    goto out;

  if (!read_node_list (&config_dir_res.file))
    goto out;

  if (!read_volume_list (&config_dir_res.file))
    goto out;

  /* Config directory may have changed so lookup it again.  */
  r = zfs_extended_lookup (&config_dir_res, &root_fh, "config");
  if (r != ZFS_OK)
    goto out;

  if (!read_user_list (&config_dir_res.file))
    goto out;

  if (!read_group_list (&config_dir_res.file))
    goto out;

  r = zfs_extended_lookup (&user_dir_res, &config_dir_res.file, "user");
  if (r == ZFS_OK)
    {
      if (!read_user_mapping (&user_dir_res.file, 0))
	goto out;
      if (!read_user_mapping (&user_dir_res.file, this_node->id))
	goto out;
    }

  r = zfs_extended_lookup (&group_dir_res, &config_dir_res.file, "group");
  if (r == ZFS_OK)
    {
      if (!read_group_mapping (&group_dir_res.file, 0))
	goto out;
      if (!read_group_mapping (&group_dir_res.file, this_node->id))
	goto out;
    }

  /* Reread the updated configuration about nodes and volumes.  */
  vol = volume_lookup (VOLUME_ID_CONFIG);
  if (!vol)
    goto out;

  if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&vol->mutex);

      if (!read_node_list (&config_dir_res.file))
	goto out;

      if (!read_volume_list (&config_dir_res.file))
	goto out;
    }
  else
    zfsd_mutex_unlock (&vol->mutex);

  if (!fix_config ())
    goto out;

  /* Let the main thread run.  */
  t->retval = ZFS_OK;
  reading_cluster_config = false;
  pthread_kill (main_thread, SIGUSR1);

  /* Change state to IDLE.  */
  zfsd_mutex_lock (&t->mutex);
  if (t->state == THREAD_DYING)
    {
      zfsd_mutex_unlock (&t->mutex);
      goto dying;
    }
  t->state = THREAD_IDLE;
  zfsd_mutex_unlock (&t->mutex);
  
  /* Reread parts of configuration when notified.  */
  varray_create (&v, sizeof (uint32_t), 4);
  while (1)
    {
      string relative_path;
      uint32_t from_sid, sid;
      unsigned int i;
      node nod;
      void **slot;

      /* Wait until we are notified.  */
      semaphore_down (&t->sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
	abort ();
#endif
      if (get_thread_state (t) == THREAD_DYING)
	break;

      while (get_reread_config_request (&relative_path, &from_sid))
	{
	  /* First send the reread_config request to slave nodes.  */
	  vol = volume_lookup (VOLUME_ID_CONFIG);
	  if (!vol)
	    {
	      terminate ();
	      break;
	    }
#ifdef ENABLE_CHECKING
	  if (!vol->slaves)
	    abort ();
#endif

	  VARRAY_USED (v) = 0;
	  HTAB_FOR_EACH_SLOT (vol->slaves, slot)
	    {
	      node nod = (node) *slot;

	      zfsd_mutex_lock (&node_mutex);
	      zfsd_mutex_lock (&nod->mutex);
	      if (nod->id != from_sid)
		VARRAY_PUSH (v, nod->id, uint32_t);
	      zfsd_mutex_unlock (&nod->mutex);
	      zfsd_mutex_unlock (&node_mutex);
	    }
	  zfsd_mutex_unlock (&vol->mutex);

	  for (i = 0; i < VARRAY_USED (v); i++)
	    {
	      sid = VARRAY_ACCESS (v, i, uint32_t);
	      nod = node_lookup (sid);
	      if (nod)
		remote_reread_config (&relative_path, nod);
	    }

	  /* Then reread the configuration.  */
	  if (!reread_config_file (&relative_path))
	    {
	      terminate ();
	      break;
	    }
	}
    }
  varray_destroy (&v);

dying:
  set_thread_state (t, THREAD_DEAD);
  return NULL;

out:
  t->retval = ZFS_OK + 1;
  reading_cluster_config = false;
  pthread_kill (main_thread, SIGUSR1);
  set_thread_state (t, THREAD_DEAD);
  return NULL;
}

/* Read global configuration of the cluster from config volume.  */

static bool
read_global_cluster_config (void)
{
  semaphore_init (&config_reader_data.sem, 0);
  network_worker_init (&config_reader_data);
  config_reader_data.from_sid = this_node->id;
  config_reader_data.state = THREAD_BUSY;

  reading_cluster_config = true;
  if (pthread_create (&config_reader_data.thread_id, NULL, config_reader,
		      &config_reader_data))
    {
      message (-1, stderr, "pthread_create() failed\n");
      config_reader_data.state = THREAD_DEAD;
      config_reader_data.thread_id = 0;
      reading_cluster_config = false;
      network_worker_cleanup (&config_reader_data);
      semaphore_destroy (&config_reader_data.sem);
      return false;
    }

  /* Workaround valgrind bug (PR/77369),  */
  while (reading_cluster_config)
    {
      /* Sleep gets interrupted by the signal.  */
      sleep (1000000);
    }

  return config_reader_data.retval == ZFS_OK;
}

/* Read configuration of the cluster - nodes, volumes, ... */

bool
read_cluster_config (void)
{
  if (!read_local_cluster_config (&node_config))
    return false;

  if (!init_config ())
    return false;

  if (!read_global_cluster_config ())
    return false;

  return true;
}

/* Read configuration from FILE and using this information read configuration
   of node and cluster.  Return true on success.  */

bool
read_config_file (const char *file)
{
  FILE *f;
  char line[LINE_SIZE + 1];
  char *key, *value;
  int line_num;

  /* Get the name of local node.  */
  set_node_name ();

  /* Set default local user/group.  */
  set_default_uid_gid ();

  /* Default values.  */
  set_str (&kernel_file_name, "/dev/zfs");
  set_str (&node_config, "/etc/zfs");

  /* Read the config file.  */
  f = fopen (file, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", file, strerror (errno));
      return false;
    }

  message (2, stderr, "Reading configuration file '%s'\n", file);
  line_num = 0;
  while (!feof (f))
    {
      int value_len;

      if (!fgets (line, sizeof (line), f))
	break;

      line_num++;
      value_len = process_line (file, line_num, line, &key, &value);

      if (*key)		/* There was a configuration directive on the line.  */
	{
	  if (value_len)
	    {
	      /* Configuration options which may have a value.  */

	      if (strncasecmp (key, "hostname", 9) == 0)
		{
		  set_string_with_length (&node_host_name, value, value_len);
		  message (1, stderr, "HostName = '%s'\n", value);
		}
	      else if (strncasecmp (key, "privatekey", 11) == 0)
		{
		  set_string_with_length (&private_key, value, value_len);
		  message (1, stderr, "PrivateKey = '%s'\n", value);
		}
	      else if (strncasecmp (key, "nodeconfig", 11) == 0
		       || strncasecmp (key, "nodeconfiguration", 18) == 0
		       || strncasecmp (key, "localconfig", 12) == 0
		       || strncasecmp (key, "localconfiguration", 19) == 0)
		{
		  set_string_with_length (&node_config, value, value_len);
		  message (1, stderr, "NodeConfig = '%s'\n", value);
		}
	      else if (strncasecmp (key, "defaultuser", 12) == 0)
		{
		  if (!set_default_uid (value))
		    {
		      message (0, stderr, "Unknown (local) user: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultuid", 11) == 0)
		{
		  if (sscanf (value, "%" PRIu32, &default_node_uid) != 1)
		    {
		      message (0, stderr, "Not an unsigned number: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultgroup", 13) == 0)
		{
		  if (!set_default_gid (value))
		    {
		      message (0, stderr, "Unknown (local) group: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultgid", 11) == 0)
		{
		  if (sscanf (value, "%" PRIu32, &default_node_gid) != 1)
		    {
		      message (0, stderr, "Not an unsigned number: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "metadatatreedepth", 18) == 0)
		{
		  if (sscanf (value, "%u", &metadata_tree_depth) != 1)
		    {
		      message (0, stderr, "Not an unsigned number: %s\n",
			       value);
		    }
		  else
		    {
		      if (metadata_tree_depth > MAX_METADATA_TREE_DEPTH)
			metadata_tree_depth = MAX_METADATA_TREE_DEPTH;
		      message (1, stderr, "MetadataTreeDepth = %u\n",
			       metadata_tree_depth);
		    }
		}
	      else
		{
		  message (0, stderr, "%s:%d: Unknown option: '%s'\n",
			   file, line_num, key);
		  return false;
		}
	    }
	  else
	    {
	      /* Configuration options which may have no value.  */

	      /* Configuration options which require a value.  */
	      if (strncasecmp (key, "nodename", 9) == 0
		  || strncasecmp (key, "privatekey", 11) == 0
		  || strncasecmp (key, "nodeconfig", 11) == 0
		  || strncasecmp (key, "nodeconfiguration", 18) == 0
		  || strncasecmp (key, "localconfig", 12) == 0
		  || strncasecmp (key, "localconfiguration", 19) == 0
		  || strncasecmp (key, "clusterconfig", 14) == 0
		  || strncasecmp (key, "clusterconfiguration", 21) == 0
		  || strncasecmp (key, "defaultuser", 12) == 0
		  || strncasecmp (key, "defaultuid", 11) == 0
		  || strncasecmp (key, "defaultgroup", 13) == 0
		  || strncasecmp (key, "defaultgid", 11) == 0
		  || strncasecmp (key, "metadatatreedepth", 18) == 0)
		{
		  message (-1, stderr, "Option '%s' requires a value.\n", key);
		}
	      else
		{
		  message (0, stderr, "%s:%d: Unknown option: '%s'\n",
			   file, line_num, key);
		  return false;
		}
	    }
	}
    }
  fclose (f);

  if (node_host_name.len == 0)
    {
      message (-1, stderr,
	       "Host name was not autodetected nor defined in configuration file.\n");
      return false;
    }

  if (default_node_uid == (uint32_t) -1)
    {
      message (-1, stderr,
	       "DefaultUser or DefaultUID was not specified,\n  'nobody' could not be used either.\n");
      return false;
    }

  if (default_node_gid == (uint32_t) -1)
    {
      message (-1, stderr,
	       "DefaultGroup or DefaultGID was not specified,\n  'nogroup' or 'nobody' could not be used either.\n");
      return false;
    }

  if (!private_key.str)
    append_file_name (&private_key, &node_config, "node_key", 8);
  if (!read_private_key (&private_key))
    return false;
  return true;
}

/* Initialize data structures in CONFIG.C.  */

void
initialize_config_c (void)
{
  zfsd_mutex_init (&reread_config_mutex);

  reread_config_pool
    = create_alloc_pool ("reread_config_pool",
			 sizeof (struct reread_config_request_def), 1022,
			 &reread_config_mutex);
}

/* Destroy data structures in CONFIG.C.  */

void
cleanup_config_c (void)
{
  zfsd_mutex_lock (&reread_config_mutex);
#ifdef ENABLE_CHECKING
  if (reread_config_pool->elts_free < reread_config_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in reread_config_pool.\n",
	     reread_config_pool->elts_allocated
	     - reread_config_pool->elts_free);
#endif
  free_alloc_pool (reread_config_pool);
  zfsd_mutex_unlock (&reread_config_mutex);
  zfsd_mutex_destroy (&reread_config_mutex);

  if (node_name.str)
    free (node_name.str);
  free (node_host_name.str);
  free (kernel_file_name.str);
  free (node_config.str);
}
