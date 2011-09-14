#include "system.h"
#include "read_config.h"
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
#include "pthread-wrapper.h"
#include "configuration.h"
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
#include "config_parser.h"
#include "config_defaults.h"
#include "config_limits.h"

string local_config;

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

	      if (strncasecmp (key, "localconfig", 12) == 0
		  || strncasecmp (key, "localconfiguration", 19) == 0)
		{
		  set_string_with_length (&local_config, value, value_len);
		  message (LOG_INFO, FACILITY_CONFIG, "LocalConfig = '%s'\n", value);
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

  return true;
}


/*! Process line LINE number LINE_NUM from file FILE_NAME.
   Return 0 if we should continue reading lines from file.  */

int
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

bool
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

/*! Read local info about volumes.
    \param path Path where local configuration is stored.
    \param reread True if we are rereading the local volume info.  */

bool 
read_local_volume_info (string *path, bool reread)
{
  int line_num;
  char *file;
  FILE *f;
  string parts[3];
  char line[LINE_SIZE + 1];

  file = xstrconcat (3, path->str, DIRECTORY_SEPARATOR ,"volume_info");
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
#ifndef	ENABLE_LOCAL_PATH
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


