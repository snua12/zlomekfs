/*! \file
    \brief Configuration.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

/*! Data for config reader thread.  */
thread config_reader_data;

/*! Semaphore for managing the reread request queue.  */
semaphore config_sem;

/*! File used to communicate with kernel.  */
string kernel_file_name;

/*! Directory with local node configuration. */
static string local_config;

/*! File with private key.  */
static string private_key;

/*! Node which the local node should fetch the global configuration from.  */
char *config_node;

/*! mlockall() zfsd  .*/
bool mlock_zfsd;

typedef struct reread_config_request_def *reread_config_request;
/*! \brief Element of list of requests for config reread.  */
struct reread_config_request_def
{
  /*! Next element in the chain.  */
  reread_config_request next;

  /*! Path relative to root of config volume.  */
  string relative_path;

  /*! Node which the request came from.  */
  uint32_t from_sid;
};

/*! First and last element of the chain of requests for rereading
   configuration.  */
static reread_config_request reread_config_first;
static reread_config_request reread_config_last;

/*! Alloc pool for allocating nodes of reread config chain.  */
static alloc_pool reread_config_pool;

/*! Mutex protecting the reread_config chain and alloc pool.  */
static pthread_mutex_t reread_config_mutex;

/*! Process one line of configuration file.  Return the length of value.  */
//TODO: create normal parser with flex / bison
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
      *value = NULL;
      message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Option '%s' has no value\n",
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
      message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Option '%s' has no value\n",
	       file, line_num, *key);
      return 0;
    }
  return dest - *value;
}

/*! Split the line by ':', trim the resulting parts, fill up to N parts
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

/*! Set default node UID to UID of user NAME.  Return true on success.  */

static bool
set_default_uid (const char *name)
{
  struct passwd *pwd;

  pwd = getpwnam (name);
  if (!pwd)
    return false;

  default_node_uid = pwd->pw_uid;
  return true;
}

/*! Set default node GID to GID of group NAME.  Return true on success.  */

static bool
set_default_gid (const char *name)
{
  struct group *grp;

  grp = getgrnam (name);
  if (!grp)
    return false;

  default_node_gid = grp->gr_gid;
  return true;
}

/*! Set default local user/group.  */

void
set_default_uid_gid (void)
{
  set_default_uid ("nobody");
  if (!set_default_gid ("nogroup"))
    set_default_gid ("nobody");
}

static bool
read_private_key (string *key_file)
{

  free (key_file->str);
  return true;
}

/*! Initialize local node so that we could read configuration.  */

static void
init_this_node (void)
{
  node nod;

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (this_node_id, &node_name, &node_name);
  zfsd_mutex_unlock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
}

/*! Read local info about volumes.
    \param path Path where local configuration is stored.
    \param reread True if we are rereading the local volume info.  */

static bool 
read_local_volume_info (string *path, bool reread)
{
  int line_num;
  char *file;
  FILE *f;
  string parts[3];
  char line[LINE_SIZE + 1];

  file = xstrconcat (2, path->str, "/volume_info");
  f = fopen (file, "rt");
  if (!f)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: %s\n", file, strerror (errno));
      free (file);
      return false;
    }

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
	      message (LOG_ERROR, FACILITY_CONFIG, "%s:%d: Wrong format of line\n", file,
		       line_num);
	    }
	  else if (id == 0 || id == (uint32_t) -1)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG,
		       "%s:%d: Volume ID must not be 0 or %" PRIu32 "\n",
		       file, line_num, (uint32_t) -1);
	    }
#ifdef	DISABLE_LOCAL_PATH
	  else if (parts[1].str[0] != '/')
	    {
	      message (LOG_ERROR, FACILITY_CONFIG,
		       "%s:%d: Local path must be an absolute path\n",
		       file, line_num);
	    }
#endif
	  else
	    {
	      zfsd_mutex_lock (&fh_mutex);
	      zfsd_mutex_lock (&volume_mutex);
	      if (reread)
		{
		  vol = volume_lookup_nolock (id);
		  if (!vol)
		    {
		      zfsd_mutex_unlock (&volume_mutex);
		      zfsd_mutex_unlock (&fh_mutex);
		      continue;
		    }
		  vol->marked = false;
		}
	      else
		vol = volume_create (id);
	      zfsd_mutex_unlock (&volume_mutex);

	      if (volume_set_local_info (&vol, &parts[1], size_limit))
		{
		  if (vol)
		    zfsd_mutex_unlock (&vol->mutex);
		}
	      else
		{
		  message (LOG_ERROR, FACILITY_CONFIG, "Could not set local information"
			   " about volume with ID = %" PRIu32 "\n", id);
		  volume_delete (vol);
		}
	      zfsd_mutex_unlock (&fh_mutex);
	    }
	}
      else
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%d: Wrong format of line\n", file,
		   line_num);
	}
    }

  free (file);
  fclose (f);
  return true;
}

/*! Reread local info about volumes.
    \param path Path where local configuration is stored.  */

static bool 
reread_local_volume_info (string *path)
{
  mark_all_volumes ();

  if (!read_local_volume_info (path, true))
    return false;

  delete_dentries_of_marked_volumes ();

  return true;
}

/*! Read ID and name of local node and local paths of volumes.  */

static bool
read_local_cluster_config (string *path)
{
  char *file;
  FILE *f;
  string parts[3];
  char line[LINE_SIZE + 1];

  if (path->str == 0)
    {
      message (LOG_CRIT, FACILITY_CONFIG,
	       "The directory with configuration of local node is not specified"
	       "in configuration file.\n");
      return false;
    }
  message (LOG_NOTICE, FACILITY_CONFIG, "Reading configuration of local node\n");

  /* Read ID of local node.  */
  file = xstrconcat (2, path->str, "/this_node");
  f = fopen (file, "rt");
  if (!f)
    {
      message (LOG_CRIT, FACILITY_CONFIG, "%s: %s\n", file, strerror (errno));
      free (file);
      return false;
    }
  if (!fgets (line, sizeof (line), f))
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: Could not read a line\n", file);
      free (file);
      fclose (f);
      return false;
    }
  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &this_node_id) != 1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s: Could not read node ID\n", file);
	  free (file);
	  fclose (f);
	  return false;
	}
      if (this_node_id == 0 || this_node_id == (uint32_t) -1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s: Node ID must not be 0 or %" PRIu32 "\n",
		   file, (uint32_t) -1);
	  free (file);
	  fclose (f);
	  return false;
	}
      if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s: Node name must not be empty\n", file);
	  free (file);
	  fclose (f);
	  return false;
	}
      xstringdup (&node_name, &parts[1]);
    }
  else
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s:1: Wrong format of line\n", file);
      free (file);
      fclose (f);
      return false;
    }
  free (file);
  fclose (f);

  init_this_node ();

  if (!read_local_volume_info (path, false))
    return false;

  return true;
}

/*! Initialize config volume so that we could read configuration.  */

static bool
init_config_volume (void)
{
  volume vol;

  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_lookup_nolock (VOLUME_ID_CONFIG);
  if (!vol)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "Config volume (ID == %" PRIu32 ") does not exist.\n",
	       VOLUME_ID_CONFIG);
      goto out;
    }

  if (config_node)
    {
      string parts[3];
      uint32_t sid;
      node nod;
      string path;

      if (split_and_trim (config_node, 3, parts) == 3)
	{
	  if (sscanf (parts[0].str, "%" PRIu32, &sid) != 1)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "Wrong format of node option\n");
	      goto out_usage;
	    }
	  else if (sid == 0 || sid == (uint32_t) -1)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "Node ID must not be 0 or %" PRIu32 "\n",
		       (uint32_t) -1);
	      goto out_usage;
	    }
	  else if (sid == this_node_id)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "The ID of the config node must be "
		       "different from the ID of the local node\n");
	      goto out_usage;
	    }
	  else if (parts[1].len == 0)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "Node name must not be empty\n");
	      goto out_usage;
	    }
	  else if (parts[1].len == node_name.len
		   && strcmp (parts[1].str, node_name.str) == 0)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "The name of the config node must be "
		       "different from the name of the local node\n");
	      goto out_usage;
	    }
	  else if (parts[2].len == 0)
	    {
	      message (LOG_ERROR, FACILITY_CONFIG, "Node host name must not be empty\n");
	      goto out_usage;
	    }
	  else
	    {
	      /* Create the node and set it as master of config volume.  */
	      zfsd_mutex_lock (&node_mutex);
	      nod = node_create (sid, &parts[1], &parts[2]);
	      zfsd_mutex_unlock (&nod->mutex);
	      zfsd_mutex_unlock (&node_mutex);

	      volume_set_common_info_wrapper (vol, "config", "/config", nod);
	      xstringdup (&path, &vol->local_path);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&volume_mutex);
	      zfsd_mutex_unlock (&fh_mutex);

	      /* Recreate the directory where config volume is cached.  */
	      recursive_unlink (&path, VOLUME_ID_VIRTUAL, false, false, false);
	      zfsd_mutex_lock (&fh_mutex);
	      vol = volume_lookup (VOLUME_ID_CONFIG);
#ifdef ENABLE_CHECKING
	      if (!vol)
		abort ();
#endif
	      if (volume_set_local_info (&vol, &path, vol->size_limit))
		{
		  if (vol)
		    zfsd_mutex_unlock (&vol->mutex);
		}
	      else
		{
		  zfsd_mutex_unlock (&vol->mutex);
		  message (LOG_CRIT, FACILITY_CONFIG, "Could not initialize config volume.\n");
		  goto out_fh;
		}
	      zfsd_mutex_unlock (&fh_mutex);

	      free (config_node);
	      config_node = NULL;
	    }
	}
      else
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "Wrong format of node option\n");
	  goto out_usage;
	}
    }
  else
    {
      volume_set_common_info_wrapper (vol, "config", "/config", this_node);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }
  return true;

out_usage:
  zfsd_mutex_unlock (&vol->mutex);
  usage();

out:
  zfsd_mutex_unlock (&volume_mutex);

out_fh:
  zfsd_mutex_unlock (&fh_mutex);
  destroy_all_volumes ();
  return false;
}

/*! Read file FH by lines and call function PROCESS for each line.  */

static bool
process_file_by_lines (zfs_fh *fh, const char *file_name,
		       int (*process) (char *, const char *, unsigned int,
				       void *),
		       void *data)
{
  read_res res;
  char buf[ZFS_MAXDATA];
  unsigned int pos, i, line_num;
  uint32_t end;
  uint64_t offset;
  zfs_cap cap;
  int32_t r;

  r = zfs_open (&cap, fh, O_RDONLY);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: open(): %s\n", file_name, zfs_strerror (r));
      return false;
    }

  line_num = 1;
  pos = 0;
  offset = 0;
  for (;;)
    {
      res.data.buf = buf + pos;
      r = zfs_read (&res, &cap, offset, ZFS_MAXDATA - pos, true);
      if (r != ZFS_OK)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s: read(): %s\n", file_name, zfs_strerror (r));
	  return false;
	}

      if (res.data.len == 0)
	break;

      offset += res.data.len;
      end = pos + res.data.len;
      for (pos = 0, i = 0; pos < end; pos = i + 1)
	{
	  for (i = pos; i < end; i++)
	    if (buf[i] == '\n')
	      {
		buf[i] = 0;
		if ((*process) (buf + pos, file_name, line_num, data) != 0)
		  goto finish;
		line_num++;
		break;
	      }
	  if (i == end)
	    break;
	}

      if (pos == 0 && i == ZFS_MAXDATA)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Line too long\n", file_name, line_num);
	  goto out;
	}
      if (pos > 0)
	{
	  memmove (buf, buf + pos, end - pos);
	  pos = end - pos;
	}
      else
	{
	  /* The read block does not contain new line.  */
	  pos = end;
	}
    }

finish:
  r = zfs_close (&cap);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: close(): %s\n", file_name, zfs_strerror (r));
      return false;
    }

  return true;

out:
  zfs_close (&cap);
  return false;
}

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_node (char *line, const char *file_name, unsigned int line_num,
		   ATTRIBUTE_UNUSED void *data)
{
  string parts[3];
  uint32_t sid;
  node nod;

  if (split_and_trim (line, 3, parts) == 3)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &sid) != 1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (sid == 0 || sid == (uint32_t) -1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Node ID must not be 0 or %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Node name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[2].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Node host name must not be empty\n",
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
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of nodes from CONFIG_DIR/node_list.  */

static bool
read_node_list (zfs_fh *config_dir)
{
  dir_op_res node_list_res;
  int32_t r;

  r = zfs_extended_lookup (&node_list_res, config_dir, "node_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&node_list_res.file, "config:/node_list",
				process_line_node, NULL);
}

/*! \brief Data for process_line_volume_hierarchy.  */
typedef struct volume_hierarchy_data_def
{
  varray hierarchy;
  uint32_t vid;
  uint32_t depth;
  string *name;
  string *mountpoint;
  char *master_name;
} volume_hierarchy_data;

/*! Process line LINE number LINE_NUM of volume hierarchy file FILE_NAME
   and update hierarchy DATA.  */

static int
process_line_volume_hierarchy (char *line,
			       ATTRIBUTE_UNUSED const char *file_name,
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
	      zfsd_mutex_lock (&fh_mutex);
	      zfsd_mutex_lock (&volume_mutex);
	      vol = volume_lookup_nolock (d->vid);
	      if (!vol)
		vol = volume_create (d->vid);
	      else
		{
		  if (vol->slaves)
		    htab_empty (vol->slaves);
		}

	      /* Do not set the common info of the config volume because
		 the file is still open and changing the volume master
		 from this_node to another one would cause zfs_close think
		 that it has to save the interval files.  */
	      if (d->vid != VOLUME_ID_CONFIG)
		volume_set_common_info (vol, d->name, d->mountpoint, nod);
	      else
		{
		  if (master_name)
		    d->master_name = xstrdup (master_name);
		  else
		    d->master_name = NULL;
		}

	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&volume_mutex);
	      zfsd_mutex_unlock (&fh_mutex);

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
      for (i = d->depth; i < VARRAY_USED (d->hierarchy) - 2; i++)
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

/*! Read appropriate file in VOLUME_INFO_DIR and process info about volume VID
   with name NAME and volume mountpoint MOUNTPOINT.  */

static void
read_volume_hierarchy (zfs_fh *volume_hierarchy_dir, uint32_t vid,
		       string *name, string *mountpoint)
{
  volume_hierarchy_data data;
  dir_op_res file_res;
  char *file_name, *master_name;
  string str;
  volume vol;
  node nod;
  int32_t r;

  r = zfs_extended_lookup (&file_res, volume_hierarchy_dir, name->str);
  if (r != ZFS_OK)
    return;

  varray_create (&data.hierarchy, sizeof (char *), 4);
  data.vid = vid;
  data.depth = 0;
  data.name = name;
  data.mountpoint = mountpoint;

  file_name = xstrconcat (2, "config:/volume/", name->str);
  process_file_by_lines (&file_res.file, file_name,
			 process_line_volume_hierarchy, &data);
  free (file_name);

  /* Setting the common info of config volume was postponed so set it now.  */
  if (vid == VOLUME_ID_CONFIG)
    {
      if (data.master_name)
	{
	  str.str = data.master_name;
	  str.len = strlen (data.master_name);
	  nod = node_lookup_name (&str);
	  if (nod)
	    zfsd_mutex_unlock (&nod->mutex);
	  free (data.master_name);
	}
      else
	nod = this_node;

      zfsd_mutex_lock (&fh_mutex);
      zfsd_mutex_lock (&volume_mutex);
      vol = volume_lookup_nolock (vid);
#ifdef ENABLE_CHECKING
      if (!vol)
	abort ();
#endif
      volume_set_common_info (vol, name, mountpoint, nod);

      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }

  /* Set the common volume info for nodes which were not listed in volume
     hierarchy.  */
  if (VARRAY_USED (data.hierarchy) > 0)
    {
      unsigned int i;
      string str2;
      volume vol2;
      node nod2;

      master_name = NULL;
      for (i = 0; i < VARRAY_USED (data.hierarchy); i++)
	{
	  master_name = VARRAY_ACCESS (data.hierarchy, i, char *);
	  if (master_name)
	    break;
	}

      if (master_name)
	{
	  str2.str = master_name;
	  str2.len = strlen (master_name);
	  nod2 = node_lookup_name (&str2);
	  if (!nod2)
	    goto out;
	  zfsd_mutex_unlock (&nod2->mutex);

	  zfsd_mutex_lock (&fh_mutex);
	  zfsd_mutex_lock (&volume_mutex);
	  vol2 = volume_lookup_nolock (vid);
	  if (!vol2)
	    vol2 = volume_create (vid);
	  else
	    {
	      if (!vol2->marked)
		{
		  zfsd_mutex_unlock (&vol2->mutex);
		  zfsd_mutex_unlock (&volume_mutex);
		  zfsd_mutex_unlock (&fh_mutex);
		  goto out;
		}
	      if (vol2->slaves)
		htab_empty (vol2->slaves);
	    }
	  volume_set_common_info (vol2, name, mountpoint, nod2);
	  zfsd_mutex_unlock (&vol2->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	  zfsd_mutex_unlock (&fh_mutex);
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

/*! Saved information about config volume because we need to update it after
   information about every volume was read.  */

static uint32_t saved_vid;
static string saved_name;
static string saved_mountpoint;

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_volume (char *line, const char *file_name, unsigned int line_num,
		     void *data)
{
  zfs_fh *volume_hierarchy_dir = (zfs_fh *) data;
  string parts[3];
  uint32_t vid;

  if (split_and_trim (line, 3, parts) == 3)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &vid) != 1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (vid == 0 || vid == (uint32_t) -1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Volume ID must not be 0 or %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Volume name must not be empty\n",
		   file_name, line_num);
	}
#ifdef	DISABLE_LOCAL_PATH
      else if (parts[2].str[0] != '/')
	{
	  message (LOG_ERROR, FACILITY_CONFIG,
		   "%s:%d: Volume mountpoint must be an absolute path\n",
		   file_name, line_num);
	}
#endif
      else if (vid == VOLUME_ID_CONFIG && saved_vid == 0)
	{
	  volume vol;

	  saved_vid = vid;
	  xstringdup (&saved_name, &parts[1]);
	  xstringdup (&saved_mountpoint, &parts[2]);

	  zfsd_mutex_lock (&fh_mutex);
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
	  volume_set_common_info (vol, &parts[1], &parts[2], vol->master);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	}
      else
	{
	  read_volume_hierarchy (volume_hierarchy_dir, vid, &parts[1],
				 &parts[2]);
	}
    }
  else
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of volumes from CONFIG_DIR/volume_list.  */

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
  if (!process_file_by_lines (&volume_list_res.file, "config:/volume_list",
			      process_line_volume,
			      &volume_hierarchy_res.file))
    return false;

  if (saved_vid == VOLUME_ID_CONFIG)
    {
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
      message (LOG_CRIT, FACILITY_CONFIG,
	       "config:/volume_list: Config volume does not exist\n");
      return false;
    }

  return true;
}

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_user (char *line, const char *file_name, unsigned int line_num,
		   ATTRIBUTE_UNUSED void *data)
{
  string parts[2];
  uint32_t id;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &id) != 1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (id == (uint32_t) -1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: User ID must not be %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: User name must not be empty\n",
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
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of users from CONFIG_DIR/user_list.  */

static bool
read_user_list (zfs_fh *config_dir)
{
  dir_op_res user_list_res;
  int32_t r;

  r = zfs_extended_lookup (&user_list_res, config_dir, "user_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&user_list_res.file, "config:/user_list",
				process_line_user, NULL);
}

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_group (char *line, const char *file_name, unsigned int line_num,
		    ATTRIBUTE_UNUSED void *data)
{
  string parts[2];
  uint32_t id;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (sscanf (parts[0].str, "%" PRIu32, &id) != 1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
		   file_name, line_num);
	}
      else if (id == (uint32_t) -1)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Group ID must not be %" PRIu32 "\n",
		   file_name, line_num, (uint32_t) -1);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Group name must not be empty\n",
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
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of groups from CONFIG_DIR/group_list.  */

static bool
read_group_list (zfs_fh *config_dir)
{
  dir_op_res group_list_res;
  int32_t r;

  r = zfs_extended_lookup (&group_list_res, config_dir, "group_list");
  if (r != ZFS_OK)
    return false;

  return process_file_by_lines (&group_list_res.file, "config:/group_list",
				process_line_group, NULL);
}

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_user_mapping (char *line, const char *file_name,
			   unsigned int line_num, void *data)
{
  uint32_t sid = *(uint32_t *) data;
  string parts[2];
  node nod;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (parts[0].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: ZFS user name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Node user name must not be empty\n",
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
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of user mapping.  If SID == 0 read the default user mapping
   from CONFIG_DIR/user/default else read the special mapping for node SID.  */

static bool
read_user_mapping (zfs_fh *user_dir, uint32_t sid)
{
  dir_op_res user_mapping_res;
  int32_t r;
  string node_name_;
  char *file_name;
  bool ret;

  if (sid == 0)
    {
      node_name_.str = "default";
      node_name_.len = strlen ("default");
    }
  else
    {
      node nod;

      nod = node_lookup (sid);
      if (!nod)
	return false;

      xstringdup (&node_name_, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);
    }

  r = zfs_extended_lookup (&user_mapping_res, user_dir, node_name_.str);
  if (r != ZFS_OK)
    {
      if (sid != 0)
	free (node_name_.str);
      return true;
    }

  file_name = xstrconcat (2, "config:/user/", node_name_.str);
  ret = process_file_by_lines (&user_mapping_res.file, file_name,
			       process_line_user_mapping, &sid);
  free (file_name);
  if (sid != 0)
    free (node_name_.str);
  return ret;
}

/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

static int
process_line_group_mapping (char *line, const char *file_name,
			    unsigned int line_num, void *data)
{
  uint32_t sid = *(uint32_t *) data;
  string parts[2];
  node nod;

  if (split_and_trim (line, 2, parts) == 2)
    {
      if (parts[0].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: ZFS group name must not be empty\n",
		   file_name, line_num);
	}
      else if (parts[1].len == 0)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Node group name must not be empty\n",
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
      message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Wrong format of line\n",
	       file_name, line_num);
    }

  return 0;
}

/*! Read list of group mapping.  If SID == 0 read the default group mapping
   from CONFIG_DIR/group/default else read the special mapping for node SID.  */

static bool
read_group_mapping (zfs_fh *group_dir, uint32_t sid)
{
  dir_op_res group_mapping_res;
  int32_t r;
  string node_name_;
  char *file_name;
  bool ret;

  if (sid == 0)
    {
      node_name_.str = "default";
      node_name_.len = strlen ("default");
    }
  else
    {
      node nod;

      nod = node_lookup (sid);
      if (!nod)
	return false;

      xstringdup (&node_name_, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);
    }

  r = zfs_extended_lookup (&group_mapping_res, group_dir, node_name_.str);
  if (r != ZFS_OK)
    {
      if (sid != 0)
	free (node_name_.str);
      return true;
    }

  file_name = xstrconcat (2, "config:/group/", node_name_.str);
  ret = process_file_by_lines (&group_mapping_res.file, file_name,
			       process_line_group_mapping, &sid);
  free (file_name);
  if (sid != 0)
    free (node_name_.str);
  return ret;
}

/*! Invalidate configuration.  */

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

/*! Verify configuration, fix what can be fixed. Return false if there remains
   something which can't be fixed.  */

static bool
fix_config (void)
{
  if (this_node == NULL || this_node->marked)
    return false;

  destroy_marked_volumes ();
  destroy_marked_nodes ();

  destroy_marked_user_mapping (NULL);
  destroy_marked_group_mapping (NULL);

  zfsd_mutex_lock (&this_node->mutex);
  destroy_marked_user_mapping (this_node);
  destroy_marked_group_mapping (this_node);
  zfsd_mutex_unlock (&this_node->mutex);

  destroy_marked_users ();
  destroy_marked_groups ();

  return true;
}

/*! Reread list of nodes.  */

static bool
reread_node_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return false;

  mark_all_nodes ();

  if (!read_node_list (&config_dir_res.file))
    return false;

  if (this_node == NULL || this_node->marked)
    return false;

  destroy_marked_volumes ();
  destroy_marked_nodes ();

  return true;
}

/*! Reread list of volumes.  */

static bool
reread_volume_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return false;

  mark_all_volumes ();

  if (!read_volume_list (&config_dir_res.file))
    return false;

  destroy_marked_volumes ();

  return true;
}

/*! Reread list of users.  */

static bool
reread_user_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return false;

  mark_all_users ();

  if (!read_user_list (&config_dir_res.file))
    return false;

  if (this_node)
    {
      zfsd_mutex_lock (&this_node->mutex);
      destroy_marked_user_mapping (this_node);
      zfsd_mutex_unlock (&this_node->mutex);
    }
  destroy_marked_user_mapping (NULL);
  destroy_marked_users ();

  return true;
}

/*! Reread list of groups.  */

static bool
reread_group_list (void)
{
  dir_op_res config_dir_res;
  int32_t r;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return false;

  mark_all_groups ();

  if (!read_group_list (&config_dir_res.file))
    return false;

  if (this_node)
    {
      zfsd_mutex_lock (&this_node->mutex);
      destroy_marked_group_mapping (this_node);
      zfsd_mutex_unlock (&this_node->mutex);
    }
  destroy_marked_group_mapping (NULL);
  destroy_marked_groups ();

  return true;
}

/*! Reread volume hierarchy for volume VOL.  */

static void
reread_volume_hierarchy (volume vol)
{
  dir_op_res config_dir_res;
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

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    {
      free (name.str);
      free (mountpoint.str);
      destroy_marked_volume (vid);
      return;
    }

  r = zfs_extended_lookup (&volume_hierarchy_dir_res, &config_dir_res.file,
			   "volume");
  if (r != ZFS_OK)
    {
      free (name.str);
      free (mountpoint.str);
      destroy_marked_volume (vid);
      return;
    }

  read_volume_hierarchy (&volume_hierarchy_dir_res.file, vid, &name,
			 &mountpoint);

  destroy_marked_volume (vid);
}

/*! Reread user mapping for node SID.  */

static bool
reread_user_mapping (uint32_t sid)
{
  dir_op_res config_dir_res;
  dir_op_res user_dir_res;
  int32_t r;
  node nod;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return true;

  r = zfs_extended_lookup (&user_dir_res, &config_dir_res.file, "user");
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
      destroy_marked_user_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    destroy_marked_user_mapping (nod);

  return true;
}

/*! Reread group mapping for node SID.  */

static bool
reread_group_mapping (uint32_t sid)
{
  dir_op_res config_dir_res;
  dir_op_res group_dir_res;
  int32_t r;
  node nod;

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    return true;

  r = zfs_extended_lookup (&group_dir_res, &config_dir_res.file, "group");
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
      destroy_marked_group_mapping (nod);
      zfsd_mutex_unlock (&nod->mutex);
    }
  else
    destroy_marked_group_mapping (nod);

  return true;
}

/*! Reread configuration file RELATIVE_PATH.  */

static bool
reread_config_file (string *relative_path)
{
  string name;
  char *str = relative_path->str;

  if (*str != '/')
    goto out_true;
  str++;

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

/*! Add request to reread config file DENTRY to queue.  */

void
add_reread_config_request_dentry (internal_dentry dentry)
{
  string relative_path;
  thread *t;

  build_relative_path (&relative_path, dentry);

  t = (thread *) pthread_getspecific (thread_data_key);
#ifdef ENABLE_CHECKING
  if (t == NULL)
    abort ();
#endif

  add_reread_config_request (&relative_path, t->from_sid);
}

/*! Add request to reread config file PATH on volume VOL to queue.  */

void
add_reread_config_request_local_path (volume vol, string *path)
{
  string relative_path;
  thread *t;

  local_path_to_relative_path (&relative_path, vol, path);

  t = (thread *) pthread_getspecific (thread_data_key);
#ifdef ENABLE_CHECKING
  if (t == NULL)
    abort ();
#endif

  add_reread_config_request (&relative_path, t->from_sid);
}

/*! Add request to reread config file RELATIVE_PATH to queue.
   The request came from node FROM_SID.  */

void
add_reread_config_request (string *relative_path, uint32_t from_sid)
{
  reread_config_request req;

  if (get_thread_state (&config_reader_data) != THREAD_IDLE)
    return;

  zfsd_mutex_lock (&reread_config_mutex);
  req = (reread_config_request) pool_alloc (reread_config_pool);
  req->next = NULL;
  req->relative_path = *relative_path;
  req->from_sid = from_sid;

  if (reread_config_last)
    reread_config_last->next = req;
  else
    reread_config_first = req;
  reread_config_last = req;

  zfsd_mutex_unlock (&reread_config_mutex);

  semaphore_up (&config_sem, 1);
}

/*! Get a request to reread config from queue and store the relative path of
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

/*! Has the config reader already terminated?  */
static volatile bool reading_cluster_config;

/*! Thread for reading a configuration.  */

static void *
config_reader (void *data)
{
  thread *t = (thread *) data;
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  dir_op_res config_dir_res;
  dir_op_res user_dir_res;
  dir_op_res group_dir_res;
  string relative_path;
  uint32_t from_sid;
  int32_t r;
  volume vol;
  varray v;

  thread_disable_signals ();
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Config reader");
  set_lock_info (li);

  invalidate_config ();

  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "volume_root(): %s\n", zfs_strerror (r));
      goto out;
    }

  if (!read_node_list (&config_dir_res.file))
    goto out;

  if (!read_volume_list (&config_dir_res.file))
    goto out;

  /* Config directory may have changed so lookup it again.  */
  r = zfs_volume_root (&config_dir_res, VOLUME_ID_CONFIG);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "volume_root(): %s\n", zfs_strerror (r));
      goto out;
    }

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
      uint32_t sid;
      unsigned int i;
      node nod;
      void **slot;

      /* Wait until we are notified.  */
      semaphore_down (&config_sem, 1);

#ifdef ENABLE_CHECKING
      if (get_thread_state (t) == THREAD_DEAD)
	abort ();
#endif
      if (get_thread_state (t) == THREAD_DYING)
	break;

      while (get_reread_config_request (&relative_path, &from_sid))
	{
	  if (relative_path.str == NULL)
	    {
	      /* The daemon received SIGHUP, reread the local volume info.  */
	      if (!reread_local_volume_info (&local_config))
		{
		  terminate ();
		  break;
		}
	      continue;
	    }

	  /* First send the reread_config request to slave nodes.  */
	  vol = volume_lookup (VOLUME_ID_CONFIG);
	  if (!vol)
	    {
	      free (relative_path.str);
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
	      node nod2 = (node) *slot;

	      zfsd_mutex_lock (&node_mutex);
	      zfsd_mutex_lock (&nod2->mutex);
	      if (nod2->id != from_sid)
		VARRAY_PUSH (v, nod2->id, uint32_t);
	      zfsd_mutex_unlock (&nod2->mutex);
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

  /* Free remaining requests.  */
  while (get_reread_config_request (&relative_path, &from_sid))
    if (relative_path.str != NULL)
      free (relative_path.str);

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

/*! Read global configuration of the cluster from config volume.  */

static bool
read_global_cluster_config (void)
{
  semaphore_init (&config_reader_data.sem, 0);
  network_worker_init (&config_reader_data);
  config_reader_data.from_sid = 0;
  config_reader_data.state = THREAD_BUSY;

  reading_cluster_config = true;
  if (pthread_create (&config_reader_data.thread_id, NULL, config_reader,
		      &config_reader_data))
    {
      message (LOG_CRIT, FACILITY_CONFIG, "pthread_create() failed\n");
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

/*! Read configuration of the cluster - nodes, volumes, ... */

bool
read_cluster_config (void)
{
  if (!read_local_cluster_config (&local_config))
    return false;

  if (!init_config_volume ())
    return false;

  if (!read_global_cluster_config ())
    {
      message (LOG_CRIT, FACILITY_CONFIG, "Could not read global configuration\n");
      return false;
    }

  return true;
}

/*! Verify whether the thread limits are valid.
    \param limit Thread limits.
    \param name Name of the threads.  */

static bool
verify_thread_limit (thread_limit *limit, const char *name)
{
  if (limit->min_spare > limit->max_total)
    {
      message (LOG_WARNING, FACILITY_CONFIG,
	       "MinSpareThreads.%s must be lower or equal to MaxThreads.%s\n",
	       name, name);
      return false;
    }
  if (limit->min_spare > limit->max_spare)
    {
      message (LOG_WARNING, FACILITY_CONFIG,
	       "MinSpareThreads.%s must be lower or equal to MaxSpareThreads.%s\n",
	       name, name);
      return false;
    }

  return true;
}

/*! Read configuration from FILE and using this information read configuration
   of node and cluster.  Return true on success.  */

bool
read_config_file (const char *file)
{
  FILE *f;
  char line[LINE_SIZE + 1];
  char *key, *value;
  int line_num;

  /* Set default local user/group.  */
  set_default_uid_gid ();

  /* Default values.  */
  set_str (&kernel_file_name, "/dev/zfs");
  set_str (&local_config, "/etc/zfs");
  mlock_zfsd = true;

  /* Read the config file.  */
  f = fopen (file, "rt");
  if (!f)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: %s\n", file, strerror (errno));
      return false;
    }

  message (LOG_NOTICE, FACILITY_CONFIG, "Reading configuration file '%s'\n", file);
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
	      /* Configuration options which require a value.  */

#ifdef DEBUG
	      if (strncasecmp (key, "privatekey", 11) == 0)
		{
		  set_string_with_length (&private_key, value, value_len);
		  message (LOG_DEBUG, FACILITY_CONFIG, "PrivateKey = '%s'\n", value);
		}
	      else
#endif
	      if (strncasecmp (key, "localconfig", 12) == 0
		  || strncasecmp (key, "localconfiguration", 19) == 0)
		{
		  set_string_with_length (&local_config, value, value_len);
		  message (LOG_INFO, FACILITY_CONFIG, "LocalConfig = '%s'\n", value);
		}
	      else if (strncasecmp (key, "kerneldevice", 13) == 0
		       || strncasecmp (key, "kernelfile", 11) == 0)
		{
		  set_string_with_length (&kernel_file_name, value, value_len);
		  message (LOG_INFO, FACILITY_CONFIG, "KernelDevice = '%s'\n", value);
		}
	      else if (strncasecmp (key, "mlock", 6) == 0)
		{
		  int i;

		  if (sscanf (value, "%d", &i) != 1 || (i != 0 && i != 1))
		    message (LOG_ERROR, FACILITY_CONFIG, "Invalid mlock value: %s\n", value);
		  else
		    mlock_zfsd = i != 0;
		}
	      else if (strncasecmp (key, "defaultuser", 12) == 0)
		{
		  if (!set_default_uid (value))
		    {
		      message (LOG_ERROR, FACILITY_CONFIG, "Unknown (local) user: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultuid", 11) == 0)
		{
		  if (sscanf (value, "%" PRIu32, &default_node_uid) != 1)
		    {
		      message (LOG_ERROR, FACILITY_CONFIG, "Not an unsigned number: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultgroup", 13) == 0)
		{
		  if (!set_default_gid (value))
		    {
		      message (LOG_ERROR, FACILITY_CONFIG, "Unknown (local) group: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "defaultgid", 11) == 0)
		{
		  if (sscanf (value, "%" PRIu32, &default_node_gid) != 1)
		    {
		      message (LOG_ERROR, FACILITY_CONFIG, "Not an unsigned number: %s\n",
			       value);
		    }
		}
	      else if (strncasecmp (key, "metadatatreedepth", 18) == 0)
		{
		  if (sscanf (value, "%u", &metadata_tree_depth) != 1)
		    {
		      message (LOG_ERROR, FACILITY_CONFIG, "Not an unsigned number: %s\n",
			       value);
		    }
		  else
		    {
		      if (metadata_tree_depth > MAX_METADATA_TREE_DEPTH)
			metadata_tree_depth = MAX_METADATA_TREE_DEPTH;
		      message (LOG_INFO, FACILITY_CONFIG, "MetadataTreeDepth = %u\n",
			       metadata_tree_depth);
		    }
		}
#define PROCESS_THREAD_LIMITS(KEY, LEN, ELEM)				\
	      else if (strncasecmp (key, KEY, LEN) == 0)		\
		{							\
		  uint32_t ivalue;					\
									\
		  if (sscanf (value, "%" PRIu32, &ivalue) != 1)		\
		    {							\
		      message (LOG_ERROR, FACILITY_CONFIG,				\
			       "Not an unsigned number: %s\n", value);	\
		    }							\
									\
		  if (key[LEN] == 0)					\
		    {							\
		      kernel_thread_limit.ELEM =			\
		      network_thread_limit.ELEM =			\
		      update_thread_limit.ELEM = ivalue;		\
		    }							\
		  else if (strncasecmp (key + LEN, ".kernel", 8) == 0)	\
		    {							\
		      kernel_thread_limit.ELEM = ivalue;		\
		    }							\
		  else if (strncasecmp (key + LEN, ".network", 9) == 0)	\
		    {							\
		      network_thread_limit.ELEM = ivalue;		\
		    }							\
		  else if (strncasecmp (key + LEN, ".update", 8) == 0)	\
		    {							\
		      update_thread_limit.ELEM = ivalue;		\
		    }							\
		  else							\
		    {							\
		      message (LOG_WARNING, FACILITY_CONFIG,				\
			       "%s:%d: Unknown option: '%s'\n",		\
			       file, line_num, key);			\
		      return false;					\
		    }							\
		}
	      PROCESS_THREAD_LIMITS ("maxthreads", 10, max_total)
	      PROCESS_THREAD_LIMITS ("minsparethreads", 15, min_spare)
	      PROCESS_THREAD_LIMITS ("maxsparethreads", 15, max_spare)
#undef PROCESS_THREAD_LIMITS
	      else
		{
		  message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Unknown option: '%s'\n",
			   file, line_num, key);
		  return false;
		}
	    }
	  else
	    {
	      /* Configuration options which have no value.  */

	      /* Configuration options which require a value.  */
	      if (strncasecmp (key, "localconfig", 12) == 0
		  || strncasecmp (key, "localconfiguration", 19) == 0
		  || strncasecmp (key, "kerneldevice", 13) == 0
		  || strncasecmp (key, "kernelfile", 11) == 0
		  || strncasecmp (key, "mlock", 6) == 0
		  || strncasecmp (key, "defaultuser", 12) == 0
		  || strncasecmp (key, "defaultuid", 11) == 0
		  || strncasecmp (key, "defaultgroup", 13) == 0
		  || strncasecmp (key, "defaultgid", 11) == 0
		  || strncasecmp (key, "metadatatreedepth", 18) == 0)
		{
		  message (LOG_ERROR, FACILITY_CONFIG, "Option '%s' requires a value.\n", key);
		}
	      else
		{
		  message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Unknown option: '%s'\n",
			   file, line_num, key);
		  return false;
		}
	    }
	}
    }
  fclose (f);

  if (default_node_uid == (uint32_t) -1)
    {
      message (LOG_CRIT, FACILITY_CONFIG,
	       "DefaultUser or DefaultUID was not specified,\n  'nobody' could not be used either.\n");
      return false;
    }

  if (default_node_gid == (uint32_t) -1)
    {
      message (LOG_CRIT, FACILITY_CONFIG,
	       "DefaultGroup or DefaultGID was not specified,\n  'nogroup' or 'nobody' could not be used either.\n");
      return false;
    }

  if (!verify_thread_limit (&network_thread_limit, "network")
      || !verify_thread_limit (&kernel_thread_limit, "kernel")
      || !verify_thread_limit (&update_thread_limit, "update"))
    return false;

  if (!private_key.str)
    append_file_name (&private_key, &local_config, "node_key", 8);
  if (!read_private_key (&private_key))
    return false;
  return true;
}

/*! Initialize data structures in CONFIG.C.  */

void
initialize_config_c (void)
{
  zfsd_mutex_init (&reread_config_mutex);
  semaphore_init (&config_sem, 0);

  reread_config_pool
    = create_alloc_pool ("reread_config_pool",
			 sizeof (struct reread_config_request_def), 1022,
			 &reread_config_mutex);
}

/*! Destroy data structures in CONFIG.C.  */

void
cleanup_config_c (void)
{
  zfsd_mutex_lock (&reread_config_mutex);
#ifdef ENABLE_CHECKING
  if (reread_config_pool->elts_free < reread_config_pool->elts_allocated)
    message (LOG_WARNING, FACILITY_CONFIG, "Memory leak (%u elements) in reread_config_pool.\n",
	     reread_config_pool->elts_allocated
	     - reread_config_pool->elts_free);
#endif
  free_alloc_pool (reread_config_pool);
  zfsd_mutex_unlock (&reread_config_mutex);
  zfsd_mutex_destroy (&reread_config_mutex);
  semaphore_destroy (&config_sem);

  if (node_name.str)
    free (node_name.str);
  free (kernel_file_name.str);
  free (local_config.str);
}
