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
#include "semaphore.h"
#include "node.h"
#include "volume.h"
#include "metadata.h"
#include "user-group.h"
#include "thread.h"
#include "dir.h"
#include "file.h"
#include "network.h"

#ifdef BUFSIZ
#define LINE_SIZE BUFSIZ
#else
#define LINE_SIZE 2048
#endif

/* File used to communicate with kernel.  */
string kernel_file_name;

/* Directory with node configuration. */
string node_config;

/* File with private key.  */
static string private_key;

/* ID of this node.  */
static uint32_t this_node_id;

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

  node_name.len = strlen (un.nodename);
  set_string_with_length (&node_name, un.nodename, node_name.len);
  message (1, stderr, "Autodetected node name: '%s'\n", node_name.str);
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

  if (path->str == 0)
    {
      message (0, stderr,
	       "The directory with configuration of local node is not specified"
	       "in configuration file.\n");
      return false;
    }
  message (2, stderr, "Reading configuration of local node\n");

  /* Read ID of local node.  */
  file = xstrconcat (2, path->str, "/node_id");
  f = fopen (file, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", file, strerror (errno));
      free (file);
      return false;
    }
  if (fscanf (f, "%" PRIu32, &this_node_id) != 1)
    {
      message (0, stderr, "%s: Could not read node ID\n", file);
      free (file);
      fclose (f);
      return false;
    }
  fclose (f);
  if (this_node_id == 0 || this_node_id == (uint32_t) -1)
    {
      message (0, stderr, "%s: Node ID must not be 0 or %" PRIu32, file,
	       (uint32_t) -1);
      free (file);
      return false;
    }
  free (file);

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
	  string parts[3];
	  char line[LINE_SIZE + 1];

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
  vol = volume_lookup (VOLUME_ID_CONFIG);
  if (!vol)
    {
      message (0, stderr, "Config volume (ID == %" PRIu32 " does not exist.\n",
	       VOLUME_ID_CONFIG);
      zfsd_mutex_unlock (&vd_mutex);
      goto out;
    }

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (this_node_id, &node_name);
  zfsd_mutex_unlock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);

  volume_set_common_info_wrapper (vol, "config", "/config", nod);

  zfsd_mutex_unlock (&vd_mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return true;

out:
  delete_all_volumes ();
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
  string parts[2];
  uint32_t sid;
  node nod;

  if (split_and_trim (line, 2, parts) == 2)
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
      else
	{
	  nod = try_create_node (sid, &parts[1]);
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

/* Process line LINE number LINE_NUM of volume hierarchy file FILE_NAME
   and update hierarchy DATA.  */

static int
process_line_volume_hierarchy (char *line, ATTRIBUTE_UNUSED char *file_name,
			       ATTRIBUTE_UNUSED unsigned int line_num,
			       void *data)
{
  varray *hierarchy = (varray *) data;
  char *name;
  unsigned int i;

  for (i = 0; line[i] == ' '; i++)
    ;
  if (line[i] == 0)
    return 0;

  /* Free superfluous records.  */
  while (VARRAY_USED (*hierarchy) > i)
    {
      name = VARRAY_TOP (*hierarchy, char *);
      if (name)
	free (name);
      VARRAY_POP (*hierarchy);
    }

  /* Are we processing local node?  */
  if (strncmp (line + i, node_name.str, node_name.len + 1) == 0)
    return 1;

  /* Add missing empty records.  */
  while (VARRAY_USED (*hierarchy) < i)
    {
      VARRAY_PUSH (*hierarchy, NULL, char *);
    }

  name = xstrdup (line + i);
  VARRAY_PUSH (*hierarchy, name, char *);
  return 0;
}

/* Read appropriate file in VOLUME_INFO_DIR and process info about volume VID
   with name NAME and volume mountpoint MOUNTPOINT.  */

static void
read_volume_hierarchy (zfs_fh *volume_hierarchy_dir, uint32_t vid,
		       string *name, string *mountpoint)
{
  dir_op_res file_res;
  varray hierarchy;
  char *file_name, *master_name;
  string str;
  int32_t r;
  volume vol;
  node nod;

  r = zfs_extended_lookup (&file_res, volume_hierarchy_dir, name->str);
  if (r != ZFS_OK)
    return;

  varray_create (&hierarchy, sizeof (char *), 4);
  file_name = xstrconcat (2, "config/volume/", name->str);
  if (!process_file_by_lines (&file_res.file, file_name,
			      process_line_volume_hierarchy, &hierarchy))
    {
      free (file_name);
      goto out;
    }
  free (file_name);

  master_name = NULL;
  while (VARRAY_USED (hierarchy) > 0)
    {
      master_name = VARRAY_TOP (hierarchy, char *);
      if (master_name)
	break;
      VARRAY_POP (hierarchy);
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
      vol = volume_lookup (vid);
      if (!vol)
	{
	  zfsd_mutex_lock (&volume_mutex);
	  vol = volume_create (vid);
	  zfsd_mutex_unlock (&volume_mutex);
	}
      volume_set_common_info (vol, name, mountpoint, nod);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd_mutex);
    }

out:
  while (VARRAY_USED (hierarchy) > 0)
    {
      master_name = VARRAY_TOP (hierarchy, char *);
      if (master_name)
	free (master_name);
      VARRAY_POP (hierarchy);
    }
  varray_destroy (&hierarchy);
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
      else if (parts[2].str[0] != '/')
	{
	  message (0, stderr,
		   "%s:%d: Volume mountpoint must be an absolute path\n",
		   file_name, line_num);
	}
      else if (vid == VOLUME_ID_CONFIG && saved_vid == 0)
	{
	  saved_vid = vid;
	  xstringdup (&saved_name, &parts[1]);
	  xstringdup (&saved_mountpoint, &parts[2]);
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
      read_volume_hierarchy (&volume_hierarchy_res.file, saved_vid,
			     &saved_name, &saved_mountpoint);
      free (saved_name.str);
      free (saved_mountpoint.str);
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

/* Read list of user mapping.  If NOD is NULL read the default user mapping
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

/* Read list of group mapping.  If NOD is NULL read the default group mapping
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

/* Has the config reader already terminated?  */
static volatile bool config_reader_terminated;

/* Thread for reading a configuration.  */

static void *
config_reader (void *data)
{
  lock_info li[MAX_LOCKED_FILE_HANDLES];
  dir_op_res config_dir_res;
  dir_op_res user_dir_res;
  dir_op_res group_dir_res;
  int32_t r;
  volume vol;

  thread_disable_signals ();
  pthread_setspecific (thread_data_key, data);
  pthread_setspecific (thread_name_key, "Config reader");
  set_lock_info (li);

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
  if (vol)
    {
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
    }

  config_reader_terminated = true;
  pthread_kill (main_thread, SIGUSR1);
  return NULL;

out:
  config_reader_terminated = true;
  pthread_kill (main_thread, SIGUSR1);
  return (void *) 1;
}

/* Read global configuration of the cluster from config volume.  */

static bool
read_global_cluster_config (void)
{
  pthread_t config_reader_id;
  thread config_reader_data;

  semaphore_init (&config_reader_data.sem, 0);
  network_worker_init (&config_reader_data);
  config_reader_data.from_sid = this_node->id;

  config_reader_terminated = false;
  if (pthread_create (&config_reader_id, NULL, config_reader,
		      &config_reader_data))
    {
      message (-1, stderr, "pthread_create() failed\n");
      config_reader_id = 0;
      config_reader_terminated = true;
      network_worker_cleanup (&config_reader_data);
      semaphore_destroy (&config_reader_data.sem);
      return false;
    }
  else
    {
      void *retval;

      /* Workaround valgrind bug (PR/77369),  */
      while (!config_reader_terminated)
	{
	  /* Sleep gets interrupted by the signal.  */
	  sleep (1000000);
	}

      pthread_join (config_reader_id, &retval);
      network_worker_cleanup (&config_reader_data);
      semaphore_destroy (&config_reader_data.sem);

      return retval == NULL;
    }

  return true;
}

/* Invalidate configuration.  */

static void
invalidate_config (void)
{

}

/* Verify configuration, fix what can be fixed. Return false if there remains
   something which can't be fixed.  */

static bool
fix_config (void)
{

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

	      if (strncasecmp (key, "nodename", 9) == 0)
		{
		  node_name.len = value_len;
		  set_string_with_length (&node_name, value, value_len);
		  message (1, stderr, "NodeName = '%s'\n", value);
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

  if (node_name.len == 0)
    {
      message (-1, stderr,
	       "Node name was not autodetected nor defined in configuration file.\n");
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
    append_file_name (&private_key, &node_config, node_name.str, node_name.len);
  if (!read_private_key (&private_key))
    return false;
  return true;
}

/* Read configuration of the cluster - nodes, volumes, ... */

bool
read_cluster_config (void)
{
  invalidate_config ();

  if (!read_local_cluster_config (&node_config))
    return false;

  if (!init_config ())
    return false;

  if (!read_global_cluster_config ())
    return false;

  if (!fix_config ())
    return false;

  return true;
}

/* Destroy data structures in CONFIG.C.  */

void
cleanup_config_c (void)
{
  free (node_name.str);
  free (kernel_file_name.str);
  free (node_config.str);
}
