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
#include <sys/utsname.h>
#include "pthread.h"
#include "config.h"
#include "constant.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "metadata.h"
#include "user-group.h"

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
read_private_key (ATTRIBUTE_UNUSED string *filename)
{

  return true;
}

static bool
read_local_cluster_config (string *path)
{
  char *volumes;
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

  volumes = xstrconcat (2, path->str, "/volume_info");
  f = fopen (volumes, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", volumes, strerror (errno));
      free (volumes);
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
		  message (0, stderr, "%s:%d: Wrong format of line\n", volumes,
			   line_num);
		}
	      else if (id == 0 || id == (uint32_t) -1)
		{
		  message (0, stderr,
			   "%s:%d: Volume ID must not be 0 or %" PRIu32,
			   volumes, line_num, (uint32_t) -1);
		}
	      else if (parts[1].str[0] != '/')
		{
		  message (0, stderr,
			   "%s:%d: Local path must be an absolute path\n",
			   volumes, line_num);
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
	      message (0, stderr, "%s:%d: Wrong format of line\n", volumes,
		       line_num);
	    }
	}
      fclose (f);
    }

  free (volumes);
  return true;
}

static bool
read_global_cluster_config (void)
{

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

/* Initialize data structures which are needed for reading configuration.  */

bool
init_config (void)
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

  if (!read_global_cluster_config ())
    return false;

  if (!fix_config ())
    return false;

  return true;
}
