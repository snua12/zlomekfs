#include "system.h"
#include "log.h"
#include "config_parser.h"
#include "user-group.h"
#include "dir.h"
#include "config_group_mapping.h"

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

bool
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

