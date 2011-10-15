#include "system.h"
#include "local_config.h"
#include "configuration.h"
#include <libconfig.h>
#include "log.h"
#include "constant.h"
#include "thread.h"
#include "metadata.h"

//TODO: code duplicity
static void config_log_error(const config_t * config)
{
	message(LOG_EMERG, FACILITY_CONFIG,
			"Failed to read config file at line %d (%s)\n",
			config_error_line(config),
			config_error_text(config));
}

//TODO: code duplicity
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

int read_volumes_local_config(config_t * config, bool reread)
{
	config_setting_t * settings = config_lookup(config, "volumes");
	if ((settings == NULL) && (config_setting_is_array(settings) != CONFIG_TRUE))
	{
		// failed to locate volumes configuration section
		message(LOG_ERROR, FACILITY_CONFIG, "Volumes local config section is missing, please add it to local config.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * volume_setting;
	int i;
	// for each volume element
	for (i = 0; (volume_setting = config_setting_get_elem(settings, i)) != NULL; ++i)
	{
		uint32_t id;
		uint64_t cache_size;
		const char * local_path;
		int rv;

		rv = config_setting_lookup_int(volume_setting, "id", (long int *) &id);
		if (rv != CONFIG_TRUE || !is_valid_volume_id(id))
		{
			// failed to read volume_setting id from zfsd.config
			message(LOG_ERROR, FACILITY_CONFIG, "Volume id config key is wrong type or is missing in local config.\n");
			return CONFIG_FALSE;
		}

		rv = config_setting_lookup_int64(volume_setting, "cache_size", (long long int *) &cache_size);
		if (rv != CONFIG_TRUE)
		{
			// failed to read volume_setting cache limit, assume 0 will be fine
			message(LOG_WARNING, FACILITY_CONFIG, "Volume cache_size key is wrong type or is missing in local config, assuming cache_size = 0.\n");
			cache_size = 0;
		}

		rv = config_setting_lookup_string(volume_setting, "local_path", &local_path);
		if (rv != CONFIG_TRUE)
		{
			// failed to get local path
			message(LOG_ERROR, FACILITY_CONFIG, "Volume local_path config keyi is wrong type or is missing in local config.\n");
			return CONFIG_FALSE;
		}

		create_volume_from_local_config(id, cache_size, local_path, reread);
	}

	return CONFIG_TRUE;
}

int read_users_local_config(config_t * config)
{
	const char * default_user;

	config_setting_t * users_settings = config_lookup(config, "users");
	if (users_settings == NULL)
	{
		//TODO: set uid to nobody
		message(LOG_ERROR, FACILITY_CONFIG, "In users local config section is missing, please add is to local config.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * setting_default_uid = config_setting_get_member(users_settings, "default_uid");
	config_setting_t * setting_default_user = config_setting_get_member(users_settings, "default_user");

	if (setting_default_uid != NULL && setting_default_user != NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In users local config is default_uid and default_user.\n");
		return CONFIG_FALSE;
	}

	if (setting_default_uid == NULL && setting_default_user == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In users local config are missing default_uid and default_user.\n");
		return CONFIG_FALSE;
	}

	if (setting_default_uid != NULL)
	{
		if (config_setting_type(setting_default_uid) != CONFIG_TYPE_INT)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In users local config default_uid is wrong type, it should be int.\n");
			return CONFIG_FALSE;
		}

		zfs_config.default_node_uid = config_setting_get_int(setting_default_uid);
		return CONFIG_TRUE;
	}

	if (config_setting_type(setting_default_user) != CONFIG_TYPE_STRING)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In users local config default_user is wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	default_user = config_setting_get_string(setting_default_user);
	if (default_user == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In users local config default_user is wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	if (!set_default_uid(default_user))
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"In users local config is unknown (local) user.\n", default_user);
		return CONFIG_FALSE;
	}

	return CONFIG_TRUE;
}

int read_groups_local_config(config_t * config)
{
	const char * default_group;

	config_setting_t * groups_settings = config_lookup(config, "groups");
	if (groups_settings == NULL)
	{
		//TODO: set nogroup or nobody as gid
		message(LOG_ERROR, FACILITY_CONFIG, "In groups local config section is missing, please add is to local config.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * setting_default_gid = config_setting_get_member(groups_settings, "default_gid");
	config_setting_t * setting_default_group = config_setting_get_member(groups_settings, "default_group");

	if (setting_default_gid != NULL && setting_default_group != NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In groups local config is default_gid and default_groups.\n");
		return CONFIG_FALSE;
	}

	if (setting_default_gid == NULL && setting_default_group == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In groups local config are missing default_uid and default_group.\n");
		return CONFIG_FALSE;
	}

	if (setting_default_gid != NULL)
	{
		if (config_setting_type(setting_default_gid) != CONFIG_TYPE_INT)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In groups local config default_gid is wrong type, it should be int.\n");
			return CONFIG_FALSE;
		}

		zfs_config.default_node_gid = config_setting_get_int(setting_default_gid);
		return CONFIG_TRUE;
	}

	if (config_setting_type(setting_default_group) != CONFIG_TYPE_STRING)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In groups local config default_group is wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	default_group = config_setting_get_string(setting_default_group);
	if (default_group == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In groups local config default_group is wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	if (!set_default_gid(default_group))
	{
		message(LOG_ERROR, FACILITY_CONFIG,
				"In groups local config is unknown (local) group.\n", default_group);
		return CONFIG_FALSE;
	}

	return CONFIG_TRUE;

}

int read_this_node_local_config(config_t * config)
{
	const char * local_node_name;

	config_setting_t * local_node_setting = config_lookup(config, "local_node");
	if (local_node_setting == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Local node section is missing, please add it to local config.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * setting_node_id = config_setting_get_member(local_node_setting, "id");
	config_setting_t * setting_node_name = config_setting_get_member(local_node_setting, "name");
	
	if (setting_node_id == NULL || setting_node_name == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section node name or node id are missing, please add them to local config.\n");
		return CONFIG_FALSE;
	}

	if (config_setting_type(setting_node_id) != CONFIG_TYPE_INT)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section key id has wrong type, it should be int.\n");
		return CONFIG_FALSE;
	}

	if (config_setting_type(setting_node_name) != CONFIG_TYPE_STRING)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section key name has wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	zfs_config.this_node.node_id = config_setting_get_int(setting_node_id);
	local_node_name = config_setting_get_string(setting_node_name);

	xmkstring(&(zfs_config.this_node.node_name), local_node_name);

	return CONFIG_TRUE;
}

int read_config_node_local_config(config_t * config)
{
	config_setting_t * config_node_setting = config_lookup(config, "config_node");
	if (config_node_setting == NULL)
	{
		message(LOG_WARNING, FACILITY_CONFIG, "Config node section is missing, using and config node this node");

		zfs_config.config_node.node_id = zfs_config.this_node.node_id;
		xstringdup(&zfs_config.config_node.node_name, &zfs_config.this_node.node_name);

		return CONFIG_TRUE;
	}

	config_setting_t * setting_node_id = config_setting_get_member(config_node_setting, "id");
	config_setting_t * setting_node_name = config_setting_get_member(config_node_setting, "name");
	config_setting_t * setting_node_host = config_setting_get_member(config_node_setting, "host");

	if (setting_node_id == NULL || setting_node_name == NULL || setting_node_host == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local config node section name, node id or host name is missing, please add them to local config.\n");
		return CONFIG_FALSE;
	}

	if (config_setting_type(setting_node_id) != CONFIG_TYPE_INT)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section key id has wrong type, it should be int.\n");
		return CONFIG_FALSE;
	}

	if (config_setting_type(setting_node_name) != CONFIG_TYPE_STRING)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section key name has wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	if (config_setting_type(setting_node_host) != CONFIG_TYPE_STRING)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In local node section key name has wrong type, it should be string.\n");
		return CONFIG_FALSE;
	}

	zfs_config.config_node.node_id = config_setting_get_int(setting_node_id);
	if (!is_valid_node_id(zfs_config.config_node.node_id))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Node in config node id is invalid, please fix is.\n");
		return CONFIG_FALSE;
	}

	if (zfs_config.this_node.node_id == zfs_config.config_node.node_id)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Node in config node id is same as this node id.\n");
		return CONFIG_FALSE;
	}

	const char * local_node_name = config_setting_get_string(setting_node_name);
	if (!is_valid_node_name(local_node_name))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Node name in config node is invalid.\n");
		return CONFIG_FALSE;
	}

	xmkstring(&zfs_config.config_node.node_name, local_node_name);
	if (stringeq(&zfs_config.config_node.node_name, &zfs_config.this_node.node_name))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Node name in config node is same as this node name.\n");
		return CONFIG_FALSE;
	}


	const char * local_node_host = config_setting_get_string(setting_node_host);
	if (!is_valid_host_name(local_node_host))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Host name in config node is invalid.\n");
		return CONFIG_FALSE;
	}

	xmkstring(&(zfs_config.config_node.host_name), local_node_host);


	return CONFIG_TRUE;
}

int read_system_specific_config(config_t * config)
{
	config_setting_t * system_settings = config_lookup(config, "system");
	if (system_settings == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "System config section is missing in local config\n");
		/* System config is optional section */
		return CONFIG_TRUE;
	}

	config_setting_t * member;

	/*mlock*/
	member = config_setting_get_member(system_settings, "mlock");
	if (member != NULL)
	{
		if (config_setting_type(member) != CONFIG_TYPE_BOOL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In system local config mlock key has wrong type, it should be bool.\n");
			return CONFIG_FALSE;
		}
		zfs_config.mlock_zfsd = config_setting_get_bool(member);
	}

	/*metadata_tree_depth*/
	member = config_setting_get_member(system_settings, "metadata_tree_depth");
	if (member != NULL)
	{
		if (config_setting_type(member) != CONFIG_TYPE_INT)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In system local config metadata_tree_depth key has wrong type, it should be int.\n");
			return CONFIG_FALSE;
		}

		zfs_config.metadata.metadata_tree_depth = config_setting_get_int(member);
		if (!is_valid_metadata_tree_depth(zfs_config.metadata.metadata_tree_depth))
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In system local config metadata_tree_depth key is out of range (min=%d max=%d current=%d).\n",
					MIN_METADATA_TREE_DEPTH, MAX_METADATA_TREE_DEPTH, zfs_config.metadata.metadata_tree_depth);
			return CONFIG_FALSE;
		}
	}

	return CONFIG_TRUE;
}

static int read_thread_setting(config_setting_t * setting, thread_limit * limit)
{
	config_setting_t * config_max_total = config_setting_get_member(setting, "max_total");
	config_setting_t * config_min_spare = config_setting_get_member(setting, "min_spare");
	config_setting_t * config_max_spare = config_setting_get_member(setting, "max_spare");

	if (config_max_total == NULL || config_min_spare == NULL || config_max_spare == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In thread setting is missing one of theses keys: max_total, min_spare or max_spare.\n");
		return CONFIG_FALSE;
	}

	if ((config_setting_type(config_max_total) != CONFIG_TYPE_INT)
	 || (config_setting_type(config_min_spare) != CONFIG_TYPE_INT)
	 || (config_setting_type(config_max_spare) != CONFIG_TYPE_INT))
	{
		message(LOG_ERROR, FACILITY_CONFIG, "In thread setting has on of theses keys: max_total, min_spare or max_spare wrong type, it should be int.\n");
		return CONFIG_FALSE;
	}

	limit->max_total = config_setting_get_int(config_max_total);
	limit->min_spare = config_setting_get_int(config_min_spare);
	limit->max_spare = config_setting_get_int(config_max_spare);
	
	return (is_valid_thread_limit(limit, "") == true) ? CONFIG_TRUE : CONFIG_FALSE;
}

int read_threads_config(config_t * config)
{
	int rv;
	config_setting_t * setting_threads = config_lookup(config, "threads");
	if (setting_threads == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No threads section was found in local config.\n");
		return CONFIG_TRUE;
	}

	config_setting_t * setting_thread;

	setting_thread = config_setting_get_member(setting_threads, "kernel_thread");
	if (setting_thread != NULL)
	{
		rv = read_thread_setting(setting_thread, &zfs_config.threads.kernel_thread_limit);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In threads section failed to read thread limit for kernel thread.\n");
			return CONFIG_FALSE;
		}
	}

	setting_thread = config_setting_get_member(setting_threads, "network_thread");
	if (setting_thread != NULL)
	{
		rv = read_thread_setting(setting_thread, &zfs_config.threads.network_thread_limit);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In threads section failed to read thread limit for network thread.\n");
			return CONFIG_FALSE;
		}
	}

	setting_thread = config_setting_get_member(setting_threads, "update_thread");
	if (setting_thread != NULL)
	{
		rv = read_thread_setting(setting_thread, &zfs_config.threads.update_thread_limit);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "In threads section failed to read thread limit for update thread.\n");
			return CONFIG_FALSE;
		}
	}

	return CONFIG_TRUE;
}

//TODO:
//int read_versioning_config(config_t * config)

int read_local_config(config_t * config)
{
	int rv;
	rv = read_system_specific_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read system specific config from local config.\n");
		return rv;
	}

	rv = read_threads_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read thread specific config from local config.\n");
		return rv;
	}

	rv = read_this_node_local_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read this node config from local config.\n");
		return rv;
	}

	rv = read_config_node_local_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read config node config from local config.\n");
		return rv;
	}

	rv = read_users_local_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read local users config from local config.\n");
		return rv;
	}

	rv = read_groups_local_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read local groups config from local config.\n");
		return rv;
	}

	rv = read_volumes_local_config(config, false);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read local volumes config from local config.\n");
		return rv;
	}

	return rv;
}

int read_local_config_from_file(const char * local_path)
{
	int rv;

	config_t config;
	config_init(&config);

	rv = config_read_file(&config, local_path);
	if (rv != CONFIG_TRUE)
	{
		config_log_error(&config);
		config_destroy(&config);
		return rv;
	}

	rv = read_local_config(&config);
	
	config_destroy(&config);

	return rv;
}


int read_volumes_local_config_from_file(const char * local_path, bool reread)
{
	int rv;

	config_t config;
	config_init(&config);

	rv = config_read_file(&config, local_path);
	if (rv != CONFIG_TRUE)
	{
		config_log_error(&config);
		config_destroy(&config);
		return rv;
	}

	rv = read_volumes_local_config(&config, reread);
	
	config_destroy(&config);

	return rv;

}
