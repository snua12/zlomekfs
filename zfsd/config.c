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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#include "system.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include "zfs_prot.h"
#include "config.h"
#include "memory.h"

#ifdef BUFSIZ
#define LINE_SIZE BUFSIZ
#else
#define LINE_SIZE 2048
#endif

/* The host name of local node.  */
char *node_name;

/* Directory with node configuration. */
char *node_config;

/* Direcotry with cluster configuration. */
char *cluster_config;

/* File with private key.  */
static char *private_key;

/* Create node structure and fill it with information.  */
node
node_create(char *name)
{
  node nod;
  struct hostent *he;
 
  nod = (node) xmalloc(sizeof(node));
  nod->name = xstrdup(name);
  nod->flags = 0;
  
  he = gethostbyname(name);
  if (he)
    {
      if (he->h_addrtype == AF_INET && he->h_length == sizeof(nod->addr))
	{
	  nod->flags |= NODE_ADDR_RESOLVED;
	  memcpy(&nod->addr, he->h_addr_list[0], sizeof(nod->addr));
	}
    }
  return nod;
}

/* Create volume structure and fill it with information.  */
volume
volume_create(char *name, node master, char *mountpoint)
{
  volume vol;

  vol = (volume) xmalloc(sizeof(volume));
  vol->name = xstrdup(name);
  vol->master = master;
  vol->mountpoint = xstrdup(mountpoint);
  vol->flags = 0;
}

int
volume_set_local(volume vol, int lpath_len, char *localpath)
{
}

int
volume_set_copy(volume vol, int lpath_len, char *localpath)
{
}

static void
set_string(char **destp, const char *src, int len)
{
  if (*destp)
    free(*destp);

  *destp = xmemdup(src, len + 1);
}

enum automata_states {
  STATE_NORMAL,			/* outside quotes and not after backslash */
  STATE_QUOTED,			/* inside quotes and not after backslash  */
  STATE_BACKSLASH,		/* outside quotes and after backslash */
  STATE_QUOTED_BACKSLASH	/* inside quotes and after backslash */
};

/* Process one line of configuration file.  Return the length of value.  */

static int
process_line(const char *file, const int line_num, char *line, char **key,
	     char **value)
{
  char *dest;
  enum automata_states state;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  if (*line == '#')
    {
      /* There was no key nor value.  */
      *line = 0;
      *key = line;
      *value = line;
      return 0;
    }
  
  *key = line;
  /* Skip the key.  */
  while (*line != 0 && *line != '#' && *line != ' ' && *line != '\t')
    line++;

  if (*line == '#' || *line == 0)
    {
      *line = 0;
      message(0, stderr, "%s:%d:Option ``%s'' has no value.\n",
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
      message(0, stderr, "%s:%d:Option ``%s'' has no value.\n",
	      file, line_num, *key);
      return 0;
    }
  return dest - *value;
}

/* Get the name of local node.  */
static void
get_node_name()
{
  struct utsname un;
  int len;

  if (uname(&un) != 0)
    return;

  len = strlen(un.nodename);
  set_string(&node_name, un.nodename, len);
  message(1, stderr, "Autodetected node name: ``%s''\n", node_name);
}

static int
read_private_key(const char *filename)
{
}

static int
read_local_config(const char *path)
{
  char *volumes_path;
  DIR *dir;
  FILE *f;

  if (path == NULL || *path == 0)
    {
      message(1, stderr,
	      "The directory with node configuration is not specified in configuration file.\n");
      return 0;
    }

  volumes_path = xstrconcat(2, path, "/volumes/");
  dir = opendir(volumes_path);
  if (!dir)
    {
      free(volumes_path);
      message(1, stderr,
	      "The directory with node configuration is not specified in configuration file.\n");

      return 0;
    }


  closedir(dir);
  return 1;
}

static int
read_cluster_config(const char *path)
{

}

/* Verify configuration, fix what can be fixed. Return false if there remains
   something which can't be fixed.  */

static int
verify_config()
{

  return 1;
}

/* Read configuration from FILE and using this information read configuration
   of node and cluster.  Return true on success.  */

int
read_config(const char *file)
{
  FILE *f;
  char line[LINE_SIZE + 1];
  char *key, *value;
  int line_num;

  /* Get the name of local node.  */
  get_node_name();
  
  f = fopen(file, "rt");
  if (!f)
    {
      message(-1, stderr, "Can't open config file ``%s''.\n", file);
      return 0;
    }

  message(2, stderr, "Reading configuration file ``%s''.\n", file);
  line_num = 0;
  while (!feof(f))
    {
      int value_len;

      if (!fgets(line, sizeof(line), f))
	break;

      line_num++;
      value_len = process_line(file, line_num, line, &key, &value);

      if (*key)		/* There was a configuration directive on the line.  */
	{
	  if (value_len)
	    {
	      /* Configuration options which may have a value.  */

	      if (strncasecmp(key, "nodename", 9) == 0)
		{
		  set_string(&node_name, value, value_len);
		}
	      else if (strncasecmp(key, "privatekey", 11) == 0)
		{
		  set_string(&private_key, value, value_len);
		}
	      else if (strncasecmp(key, "nodeconfig", 11) == 0
		       || strncasecmp(key, "nodeconfiguration", 18) == 0
		       || strncasecmp(key, "localconfig", 12) == 0
		       || strncasecmp(key, "localconfiguration", 19) == 0)
		{
		  set_string(&node_config, value, value_len);
		}
	      else if (strncasecmp(key, "clusterconfig", 14) == 0
		       || strncasecmp(key, "clusterconfiguration", 21) == 0)
		{
		  /* TODO: FIXME: cluster configuration is always in the same
		     ZFS directory so the parameter should not be needed.  */
		  set_string(&cluster_config, value, value_len);
		}
	      else
		{
		  message(0, stderr, "%s:%d:Unknown option: ``%s''.\n",
			  file, line_num, key);
		  return 0;
		}
	    }
	  else
	    {
	      /* Configuration options which may have no value.  */
	        {
		  message(0, stderr, "%s:%d:Unknown option: ``%s''.\n",
			  file, line_num, key);
		  return 0;
		}
	    }
	}
    }
  fclose(f);

  if (!read_private_key(private_key))
    return 0;

  if (!read_local_config(node_config))
    return 0;

  /* TODO: FIXME: cluster configuration is always in the same
     ZFS directory so the parameter should not be needed.  */
  if (!read_cluster_config(cluster_config))
    return 0;

  if (!verify_config())
    return 0;

  return 1;
}
