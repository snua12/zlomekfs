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

/* ! Verify whether the thread limits are valid. \param limit Thread limits.
   \param name Name of the threads.  */

static bool verify_thread_limit(thread_limit * limit, const char *name)
{
	if (limit->min_spare > limit->max_total)
	{
		message(LOG_WARNING, FACILITY_CONFIG,
				"MinSpareThreads.%s must be lower or equal to MaxThreads.%s\n",
				name, name);
		return false;
	}
	if (limit->min_spare > limit->max_spare)
	{
		message(LOG_WARNING, FACILITY_CONFIG,
				"MinSpareThreads.%s must be lower or equal to MaxSpareThreads.%s\n",
				name, name);
		return false;
	}

	return true;
}


static int read_config_thread_limits(config_t * config)
{
	//TODO: fixmee

	/*

	-                     kernel_thread_limit.ELEM =                        \
	-                     network_thread_limit.ELEM =                       \
	-                     update_thread_limit.ELEM = ivalue;   


	*/

	int rv;
	rv = config_lookup_int(config, "threads:max_total", (long int *) &network_thread_limit.max_total);
	rv = config_lookup_int(config, "threads:min_spare", (long int *) &network_thread_limit.min_spare);
	rv = config_lookup_int(config, "threads:max_spare", (long int *) &network_thread_limit.max_spare);

	verify_thread_limit(&network_thread_limit, "Network thread limit. \n");

	return CONFIG_TRUE;

}

static int read_config_users_and_groups(config_t * config)
{
	int rv;
	const char * default_user;
	rv = config_lookup_string(config, "users_and_groups:default_user", &default_user);
	if (rv == CONFIG_TRUE)
	{
		if (!set_default_uid(default_user))
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"Unknown (local) user: %s\n", default_user);
		}
	}

	//TODO: default_node_uid is global variable
	rv = config_lookup_int(config, "users_and_groups:default_uid", (long int *) &default_node_uid);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"Not an unsigned number: %s\n", default_node_uid);
	}

	const char * default_group;
	rv = config_lookup_string(config, "users_and_groups:default_group", &default_group);
	if (rv != CONFIG_TRUE)
	{
		if (!set_default_uid(default_group))
		{
			message(LOG_ERROR, FACILITY_CONFIG,
					"Unknown (local) user: %s\n", default_group);
		}
	}

	//TODO: default_node_gid is global variable
	rv = config_lookup_int(config, "users_and_groups:default_gid", (long int *) &default_node_gid);
	if (rv == CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"Not an unsigned number: %s\n", default_node_gid);
	}

	return CONFIG_TRUE;
}

const char * local_config_path = NULL;


static int read_config_file_new(config_t * config)
{

	/* Set default local user/group.  */
	set_default_uid_gid();

	int rv;

	int mlock;
	rv = config_lookup_bool(config, "system:mlock", &mlock);
	zfs_config.mlock_zfsd = (rv == CONFIG_TRUE && mlock  == CONFIG_TRUE);

	rv = config_lookup_int(config, "system:metadata_tree_depth", (long int *) &metadata_tree_depth);
	if (rv != CONFIG_TRUE)
	{
		metadata_tree_depth = MIN_METADATA_TREE_DEPTH;
		message(LOG_ERROR, FACILITY_CONFIG,
				"Not an unsigned number\n");
	}
	else if (metadata_tree_depth > MAX_METADATA_TREE_DEPTH)
	{
		metadata_tree_depth = MAX_METADATA_TREE_DEPTH;
		message(LOG_INFO, FACILITY_CONFIG,
				"MetadataTreeDepth = %u\n",
				metadata_tree_depth);

	}


	read_config_users_and_groups(config);
	read_config_thread_limits(config);



	return CONFIG_TRUE;
}


static void config_log_error(const config_t * config)
{
	message(LOG_EMERG, FACILITY_CONFIG,
			"Failed to read config file at line %d (%s)\n",
			config_error_line(config),
			config_error_text(config));
}

/* ! Read configuration from FILE and using this information read
   configuration of node and cluster.  Return true on success.  */

bool read_config_file(const char * file)
{
	//TODO: this is hotfix

	local_config_path = file;

	int rv;
	config_t config;
	config_init(&config);

	rv = config_read_file(&config, file);
	if(rv != CONFIG_TRUE)
	{
		config_log_error(&config);
		config_destroy(&config);
		return false;
	}

	rv = read_config_file_new(&config);
	
	config_destroy(&config);

	return (rv == CONFIG_TRUE);
}




static bool isValidVolumeID(uint32_t id)
{
	return (id != 0) && (id != (uint32_t) -1);
}

static bool create_volume_from_local_config(uint32_t id, uint64_t cache_size, const char * local_path, bool reread)
{
	volume vol = NULL;

	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);

	if (reread)
	{
		vol = volume_lookup_nolock(id);
		if (vol == NULL)
		{
			zfsd_mutex_unlock(&volume_mutex);
			zfsd_mutex_unlock(&fh_mutex);
			return FALSE;
		}
		vol->marked = false;
	}
	else
	{
		vol = volume_create(id);
	}

	zfsd_mutex_unlock(&volume_mutex);

	string local_path_string;
	xmkstring(&local_path_string, local_path);

	if (volume_set_local_info(&vol, &local_path_string, cache_size))
	{
		if (vol)
			zfsd_mutex_unlock(&vol->mutex);
	}
	else
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"Could not set local information"
				" about volume with ID = %" PRIu32 "\n", id);
		volume_delete(vol);
	}
	zfsd_mutex_unlock(&fh_mutex);

	return true;
}

/* ! Read local info about volumes. \param path Path where local configuration 
   is stored. \param reread True if we are rereading the local volume info.  */

int read_local_volume_info(config_t * config, bool reread)
{
	config_setting_t * settings = config_lookup(config, "volumes");
	if ((settings == NULL) && (config_setting_is_array(settings) != CONFIG_TRUE))
	{
		// failed to locate volumes configuration section
		return CONFIG_FALSE;
	}

	config_setting_t * volume_setting;
	int i;
	// for each volume element
	for(i = 0; (volume_setting = config_setting_get_elem(settings, i)) != NULL; ++i)
	{
		uint32_t id;
		uint64_t cache_size;
		const char * local_path;
		int rv;

		rv = config_setting_lookup_int(volume_setting, "id", (long int *) &id);
		if (rv != CONFIG_TRUE || !isValidVolumeID(id))
		{
			// failed to read volume_setting id from zfsd.config
			continue;
		}

		rv = config_setting_lookup_int64(volume_setting, "cache_size", (long long int *) &cache_size);
		if (rv != CONFIG_TRUE)
		{
			// failed to read volume_setting cache limit, assume 0 will be fine
			cache_size = 0;
		}

		rv = config_setting_lookup_string(volume_setting, "local_path", &local_path);
		if (rv != CONFIG_TRUE)
		{
			// failed to get local path
			continue;
		}

		create_volume_from_local_config(id, cache_size, local_path, reread);
	}

	return CONFIG_TRUE;
}

