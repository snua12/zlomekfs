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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include "zfs_prot.h"
#include "config.h"

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
node_create(char *name, cipher key_type, int pubkey_len, char *pubkey)
{
  int name_len, size;
  node node;
  struct hostent *he;
  int data_end = 0;
 
  name_len = strlen(name);
  size = sizeof(struct node_def) + name_len + pubkey_len;
  node = (node) malloc(size);
  if (!node)
    return NULL;

  node->name = &node->data[data_end];
  memcpy(node->name, name, name_len + 1);
  data_end += name_len + 1;
  
  node->key_type = key_type;
  node->pubkey_len = pubkey_len;
  node->pubkey = &node->data[data_end];
  if (pubkey_len)
    {
      memcpy(node->pubkey, pubkey, pubkey_len);
      data_end += pubkey_len;
    }
  node->flags = 0;
  
  he = gethostbyname(name);
  if (he)
    {
      if (he->h_addrtype == AF_INET && he->h_length == sizeof(node->addr))
	{
	  node->flags |= NODE_ADDR_RESOLVED;
	  memcpy(node->addr, he->h_addr_list[0], sizeof(node->addr));
	}
    }
  return node;
}

/* Create volume structure and fill it with information.  */
volume
volume_create(char *name, node master, char *location, data_end)
{
  int name_len, loc_len, lpath_len, size;
  volume volume;

  name_len = strlen(name);
  loc_len = strlen(location);
  size = sizeof(struct volume_def) + data_end + name_len + loc_len;
  volume = (volume) malloc(size);
  if (!volume)
    return NULL;

  volume->master = master;
  volume->flags = 0;
  
  volume->name = &volume->data[data_end];
  memcpy(volume->name, name, name_len + 1);
  data_end += name_len + 1;

  volume->location = &volume->data[data_end];
  memcpy(volume->location, location, loc_len + 1);
  data_end += loc_len + 1;
}

int
volume_set_local(int lpath_len, char *localpath)
{
}

int
volume_set_copy(int lpath_len, char *localpath)
{
}

static int
set_string(char **destp, char *src, int len)
{
  if (*destp)
    free(*destp);

  *destp = malloc(len + 1);
  if (!destp)
    return 0;

  memcpy(*dstp, src, len + 1);

  return 1;
}

enum automaton_states {
  STATE_NORMAL,			/* outside quotes and not after backslash */
  STATE_QUOTED,			/* inside quotes and not after backslash  */
  STATE_BACKSLASH,		/* outside quotes and after backslash */
  STATE_QUOTED_BACKSLASH	/* inside quotes and after backslash */
};

/* Process one line of configuration file.  */

static int
process_line(char *file, int line_num, char *line)
{
  char *key;
  char *val;
  char *dest;
  enum automaton_states state;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line == '#')
    return 1;
  
  key = line;
  /* Skip the key.  */
  while (*line != 0 && *line != '#' && *line != ' ' && *line != '\t')
    line++;

  if (*line == '#' || *line == 0)
    {
      *line = 0;
      message(0, stderr, "%s:%d:Option ``%s'' has no value.\n",
	      file, line_num, key);
      return 0;
    }
  *line = 0;
  line++;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  val = line;
  dest = line;

  /* Finite automaton.  */
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
	    *dest++ = *list++;
	    state = STATE_QUOTED;
	    break;
	}
    }

  /* If there was '\\' on the end of line, add it to the end of string. */
  if (state == STATE_BACKSLASH || state == STATE_QUOTED_BACKSLASH)
    *dest++ = '\\';
  *dest = 0;

  if (val == dest)
    {
      message(0, stderr, "%s:%d:Option ``%s'' has no value.\n",
	      file, line_num, key);
      return 0;
    }

  /* Compare the key.  */
  if (strncasecmp(key, "nodename", 9) == 0)
    {
      return set_string(&node_name, val, dest - val);

    }
  else if (strncasecmp(key, "privatekey", 11) == 0)
    {
      return set_string(&private_key, val, dest - val);
    }
  else if (strncasecmp(key, "nodeconfig", 11) == 0
	   || strncasecmp(key, "nodeconfiguration", 18) == 0)
    {
      return set_string(&node_config, val, dest - val);
    }
  else if (strncasecmp(key, "clusterconfig", 14) == 0
	   || strncasecmp(key, "clusterconfiguration", 21) == 0)
    {
      return set_string(&cluster_config, val, dest - val);
    }
  else
    {
      message(0, stderr, "%s:%d:Unknown option: ``%s''.\n",
	      file, line_num, key);
      return 0;
    }
}

/* Get the name of local node.  */
static int
get_node_name()
{
  struct utsname un;
  int len;
  int r;

  if (uname(&un) != 0)
    return;

  len = strlen(un.nodename);
  r = set_string(&nodename, un.nodename, len);
  if (r)
    message(1, stderr, "Autodetected node name: ``%s''\n", nodename);
  return r;
}



static void
read_private_key(char *filename)
{
}

void
read_local_config(char *path)
{

}

void
read_cluster_config(char *path)
{
}

int
read_config(char *filename)
{
  FILE *f;
  char line[LINE_SIZE];
  int line_num;
  int r = 1; 

  /* Get the name of local node.  */
  get_node_name();
  
  f = fopen(filename, "rt");
  if (!f)
    {
      message(-1, "Can't open config file %s.\n", filename);
      return 0;
    }

  message(2, "Reading configuration file %s.\n", filename);
  line_num = 0;
  while (!feof(f))
    {
      if (!fgets(line, sizeof(line), f))
	break;

      line_num++;
      if (!process_line(filename, line_num, line))
	r = 0;
    }
  fclose(f);
  if (r == 0)
    return 0;


}
