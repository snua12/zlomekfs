#include "system.h"
#include <signal.h>
#include "log.h"
#include "node.h"
#include "volume.h"
#include "user-group.h"
#include "thread.h"
#include "dir.h"
#include "node.h"
#include "volume.h"
#include "config_limits.h"
#include "config_parser.h"
#include "cluster_config.h"
#include "zfsd.h"
#include "network.h"

#include "config_group.h"
#include "config_limits.h"
#include "configuration.h"
#include "config_user_mapping.h"
#include "read_config.h"
#include "config_defaults.h"
#include "config_group_mapping.h"
#include "config_parser.h"
#include "config_user.h"
#include "config_volume.h"
#include "reread_config.h"

/*! Add request to reread config file RELATIVE_PATH to queue.
   The request came from node FROM_SID.  */


/*! Has the config reader already terminated?  */
static volatile bool reading_cluster_config;



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

/*! Read global configuration of the cluster from config volume.  */
/*! Read configuration of the cluster - nodes, volumes, ... */
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

bool
read_cluster_config (void)
{
  if (!read_local_cluster_config (&local_config))
    {
      message (LOG_CRIT, FACILITY_CONFIG, "Could not read local cluster configuration\n");
      return false;
    }

  if (!init_config_volume ())
    {
      message (LOG_CRIT, FACILITY_CONFIG, "Could not init config volume\n");
      return false;
    }

  if (!read_global_cluster_config ())
    {
      message (LOG_CRIT, FACILITY_CONFIG, "Could not read global configuration\n");
      return false;
    }

  return true;
}

