/* Configuration.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/utsname.h>
#include "pthread.h"
#include "config.h"
#include "log.h"
#include "memory.h"

#ifdef BUFSIZ
#define LINE_SIZE BUFSIZ
#else
#define LINE_SIZE 2048
#endif

/* The host name of local node.  */
char *node_name;

/* Length of host name of local node.  */
unsigned int node_name_len;

/* Directory with node configuration. */
char *node_config;

/* Direcotry with cluster configuration. */
char *cluster_config;

/* File with private key.  */
static char *private_key;

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
split_and_trim (char *line, int n, char **parts)
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
	parts[i] = start;

      /* Find the end of a part.  */
      while (*line != 0 && *line != '\n' && *line != ':')
	line++;
      colon = line;

      if (i < n)
	{
	  /* Delete white spaces at the end of a part.  */
	  while (line > start
		 && (*line == ' ' || *line == '\t'))
	    {
	      *line = 0;
	      line--;
	    }
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
get_node_name ()
{
  struct utsname un;

  if (uname (&un) != 0)
    return;

  node_name_len = strlen (un.nodename);
  set_string_with_length (&node_name, un.nodename, node_name_len);
  message (1, stderr, "Autodetected node name: '%s'\n", node_name);
}

static bool
read_private_key (const char *filename)
{

  return true;
}

static bool
read_local_cluster_config (const char *path)
{
  char *volumes;
  FILE *f;
  int line_num;

  if (path == NULL || *path == 0)
    {
      message (0, stderr,
	       "The directory with configuration of local node is not specified in configuration file.\n");
      return false;
    }
  message (2, stderr, "Reading configuration of local node\n");

  volumes = xstrconcat (2, path, "/volumes");
  f = fopen (volumes, "rt");
  if (!f)
    {
      message (-1, stderr, "%s: %s\n", volumes, strerror (errno));
      free (volumes);
    }
  else
    {
      line_num = 0;
      while (!feof (f))
	{
	  char *parts[3];
	  char line[LINE_SIZE + 1];

	  if (!fgets (line, sizeof (line), f))
	    break;

	  line_num++;
	  if (split_and_trim (line, 3, parts) == 3)
	    {
	      /* 0 ... ID
	         1 ... localpath
	         2 ... sizelimit */
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
read_global_cluster_config (const char *path)
{

  return true;
}

/* Invalidate configuration.  */

static void
invalidate_config ()
{

}

/* Verify configuration, fix what can be fixed. Return false if there remains
   something which can't be fixed.  */

static bool
fix_config ()
{

  return true;
}

/* Initialize data structures which are needed for reading configuration.  */

bool
init_config ()
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
  get_node_name ();

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
	      else if (strncasecmp (key, "clusterconfig", 14) == 0
		       || strncasecmp (key, "clusterconfiguration", 21) == 0)
		{
		  /* TODO: FIXME: cluster configuration is always in the same
		     ZFS directory so the parameter should not be needed.  */
		  set_string_with_length (&cluster_config, value, value_len);
		  message (1, stderr, "ClusterConfig = '%s'\n", value);
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
		  || strncasecmp (key, "clusterconfiguration", 21) == 0)
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

  if (!node_name || !*node_name)
    {
      message (-1, stderr,
	       "Node name was not autodetected nor defined in configuration file.\n");
      return false;
    }

  if (!private_key)
    private_key = xstrconcat (3, node_config, "/", node_name);
  if (!read_private_key (private_key))
    return false;
  return true;
}

/* Read configuration of the cluster - nodes, volumes, ... */

bool
read_cluster_config ()
{
  invalidate_config ();

  if (!read_local_cluster_config (node_config))
    return false;

  /* TODO: FIXME: cluster configuration is always in the same
     ZFS directory so the parameter should not be needed.  */
  if (!read_global_cluster_config (cluster_config))
    return false;

  if (!fix_config ())
    return false;

  return true;
}
