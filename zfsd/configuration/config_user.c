#include "system.h"
#include "config_user.h"
#include "fh.h"
#include "config_parser.h"
#include "user-group.h"
#include "dir.h"


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

bool
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


