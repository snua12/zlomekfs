#include "system.h"
#include  <string.h>
#include "memory.h"
#include "reread_config.h"
#include "read_config.h"
#include "zfs-prot.h"
#include "dir.h"
#include "node.h"
#include "config_volume.h"
#include "user-group.h"
#include "config_user_mapping.h"
#include "config_user.h"
#include "config_group.h"
#include "config_group_mapping.h"
#include "config_parser.h"
#include "config_limits.h"
#include "thread.h"
//TODO: don't share everything with configuration data
#include "configuration.h"

/*! First and last element of the chain of requests for rereading
   configuration.  */
static reread_config_request reread_config_first;
static reread_config_request reread_config_last;

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

bool
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

/*! Reread local info about volumes.
    \param path Path where local configuration is stored.  */

bool 
reread_local_volume_info (string *path)
{
  mark_all_volumes ();

  if (!read_local_volume_info (path, true))
    return false;

  delete_dentries_of_marked_volumes ();

  return true;
}


/*! Add a request to reread config into queue
*/

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

//append to queue
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

bool
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

//FIXME: probably memory leak
  reread_config_first = reread_config_first->next;
  if (reread_config_first == NULL)
    reread_config_last = NULL;

  zfsd_mutex_unlock (&reread_config_mutex);
  return true;
}


