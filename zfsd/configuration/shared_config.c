#include <libconfig.h>
#include "shared_config.h"
#include "log.h"
#include "node.h"
#include "volume.h"
#include "user-group.h"
#include "zfs_config.h"

/* ! \brief initializes volume entry */
static void volume_entry_init(volume_entry * ve)
{
	ve->id = VOLUME_ID_VIRTUAL;
	ve->name = invalid_string;
	ve->mountpoint = invalid_string;
	ve->master_name = invalid_string;
	varray_create(&ve->slave_names, sizeof(char *), 4);
}

static void volume_entry_destroy(ATTRIBUTE_UNUSED volume_entry *ve)
{
}

static bool is_valid_volume_id(uint32_t vid)
{
	return (vid != 0) && (vid != (uint32_t) -1);
}

static bool is_valid_volume_name(const char * name)
{
	// strlen(name) > 0
	return (name != NULL && name[0] != 0);
}

static bool is_valid_local_path(const char * path)
{
#ifndef	ENABLE_LOCAL_PATH
	return (path != NULL && path[0] == '/');
#else
	// strlen(path >  0)
	return (path != NULL && path[0]);
#endif

}

int read_node_list_shared_config(config_t * config)
{
	config_setting_t * node_list = config_lookup(config, "node:list");
	if (node_list == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "No node list section in shared config was found.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * node_entry;
	int i;

	for (i = 0; (node_entry = config_setting_get_elem(node_list, i)) != NULL; ++i)
	{
		
		uint32_t id;
		const char * name;
		const char * address;
		int rv;

		rv = config_setting_lookup_int(node_entry, "id", (long int *) &id);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Id config key is wrong type or is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		rv = config_setting_lookup_string(node_entry, "name", &name);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Name config key is wrong type or is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		rv = config_setting_lookup_string(node_entry, "address", &address);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Addres config key is wrong type or is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		string str_name;
		string str_address;

		xmkstring(&str_name, name);
		xmkstring(&str_address, address);

		node nod = try_create_node(id, &str_name, &str_name);
		if (nod)
			zfsd_mutex_unlock(&nod->mutex);
	}

	return CONFIG_TRUE;
}

int read_mapping_setting(config_setting_t * setting, add_mapping add)
{
	int i;
	config_setting_t * pair;
	for (i = 0; (pair = config_setting_get_elem(setting, i)) != NULL; ++i)
	{
		int rv;
		uint32_t id;

		rv = config_setting_lookup_int(pair, "id", (long int *) &id);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Id config key is wrong type or is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		const char * name;
		rv = config_setting_lookup_string(pair, "name", &name);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Name config key is wrong type or is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		//TODO: add_user(id, name);
		string str_name;
		xmkstring(&str_name, name);
		add(id, &str_name);	
	}

	return CONFIG_TRUE;
}

// user_create wrapper
static void add_user(uint32_t id, string * name)
{
	zfsd_mutex_lock(&users_groups_mutex);
	user_create(id, name);
	zfsd_mutex_unlock(&users_groups_mutex);
}

int read_user_list_shared_config(config_t * config)
{
	config_setting_t * user_list = config_lookup(config, "user:list");
	if (user_list == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No user:list section in shared config was found.\n");
		return CONFIG_TRUE;
	}
	
	int rv;
	rv = read_mapping_setting(user_list, add_user);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read user list form shared config.\n");
	}

	return rv;
}

// group_create wrapper
static void add_group(uint32_t id, string * name)
{
	zfsd_mutex_lock(&users_groups_mutex);
	group_create(id, name);
	zfsd_mutex_unlock(&users_groups_mutex);
}

//TODO; write this function based on read_user_list_shared_config
int read_group_list_shared_config(config_t * config)
{
	config_setting_t * group_list = config_lookup(config, "group:list");
	if (group_list == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No group:list section in shared config was found.\n");
		return CONFIG_TRUE;
	}

	int rv;
	rv = read_mapping_setting(group_list, add_group);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read user list from shared config.\n");
	}

	return rv;
}

static int read_pairs_setting(config_setting_t * setting)
{
	int i;
	config_setting_t * pair;
	for (i = 0; (pair = config_setting_get_elem(setting, i)) != NULL; ++i)
	{
		int rv;
		const char * local;
		const char * remote;

		rv = config_setting_lookup_string(pair, "local", &local);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read local key from pairs in shared config.\n");
			return CONFIG_FALSE;
		}

		rv = config_setting_lookup_string(pair, "remote", &remote);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read remote key from pairs in shared config.\n");
			return CONFIG_FALSE;
		}
		//TODO: fill in local and remote add(local, remote);
	}

	return CONFIG_TRUE;
}

static int read_node_mapping_setting(config_setting_t * setting)
{
	int i;
	config_setting_t * map;

	for (i = 0; (map = config_setting_get_elem(setting, i)) != NULL; ++i)
	{
		int rv;
		const char * node_key;

		rv = config_setting_lookup_string(map, "node", &node_key);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "User list node key is missing in shared configr.\n");
			return CONFIG_FALSE;
		}

		config_setting_t * config_pairs = config_setting_get_member(map, "pairs");
		if (config_pairs == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "User list pairs key is missing in shared config.\n");
			return CONFIG_FALSE;
		}

		rv = read_pairs_setting(config_pairs);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read user list pairs config from shared config.\n");
			return CONFIG_FALSE;
		}
	}

	return CONFIG_TRUE;
}

int read_user_mapping_shared_config(config_t * config)
{
	config_setting_t * user_mapping = config_lookup(config, "user:mapping");
	if (user_mapping == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No user:mapping section in shared config was found.\n");
		return CONFIG_TRUE;
	}

	return read_node_mapping_setting(user_mapping);
}

int read_group_mapping_shared_config(config_t * config)
{
	config_setting_t * user_mapping = config_lookup(config, "group:mapping");
	if (user_mapping == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No group:mapping section in shared config was found.\n");
		return CONFIG_TRUE;
	}

	return read_node_mapping_setting(user_mapping);
}

// reads volume entry from shared config
static int volume_entry_read(config_setting_t * volume_setting, volume_entry * ve)
{
	int rv;

	uint32_t id;
	rv = config_setting_lookup_int(volume_setting, "id", (long int *) &id);
	if (rv != CONFIG_TRUE)
	{
		message (LOG_ERROR, FACILITY_CONFIG, "Id confg key is missing or is wrong type in volume list.\n");
		return CONFIG_FALSE;
	}

	const char * volume_name;
	rv = config_setting_lookup_string(volume_setting, "name", &volume_name);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Name config key is missing or is wrong type in volume list.\n");
		return CONFIG_FALSE;
	}

	const char * volume_mountpoint;
	rv = config_setting_lookup_string(volume_setting, "mountpoint", &volume_mountpoint);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Mountpoint config key is missing or is wrong type in volume list.\n");
		return CONFIG_FALSE;
	}
	
	if (is_valid_volume_id(id) == false)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Volume id is invalid.\n");
		return CONFIG_FALSE;
	}

	if (is_valid_volume_name(volume_name) == false)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Volume name is invalid.\n");
		return CONFIG_FALSE;
	}

	if (is_valid_local_path(volume_mountpoint) == false)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Volume mountpoint is invalid.\n");
		return CONFIG_FALSE;
	}

	//id, name mountpoint
	ve->id = id;
	xmkstring(&ve->name, volume_name);
	xmkstring(&ve->mountpoint, volume_mountpoint);

	return CONFIG_TRUE;
}

static config_setting_t * config_setting_get_vol_layout(config_setting_t * vol_layouts, const char * vol_name)
{
	int i;
	config_setting_t * vol_layout;
	for (i = 0; (vol_layout = config_setting_get_elem(vol_layouts, i)) != NULL; ++i)
	{
		const char * config_vol_name;
		int rv = config_setting_lookup_string(vol_layout, "volume", &config_vol_name);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Missing volume key in volume layout.\n");
			return NULL;
		}
		if (strcmp(vol_name, config_vol_name) == 0)
		{
			return vol_layout;
		}
	}

	return NULL;
}

static int config_setting_get_slaves(config_setting_t * layout_tree, volume_entry * ve)
{
	int i;
	config_setting_t * child;
	for (i = 0; (child = config_setting_get_elem(layout_tree, i)) != NULL; ++i)
	{
		const char * node_name;
		int rv = config_setting_lookup_string(child, "node", &node_name);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read node name from layout tree.\n");
			return CONFIG_FALSE;
		}
		VARRAY_PUSH(ve->slave_names, xstrdup(node_name), char *);
	}

	return CONFIG_TRUE;
}

static int config_setting_process_tree(config_setting_t * layout_tree, volume_entry * ve,const char * parent_node, const char * node_name)
{
	const char * config_node_name;
	int rv = config_setting_lookup_string(layout_tree, "node", &config_node_name);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_INFO, FACILITY_CONFIG, "Failed to read node name from layout tree.\n");
		return CONFIG_TRUE;
	}

	config_setting_t * subtree = config_setting_get_member(layout_tree, "children");

	// finds our node in tree, parent is master node anc children are slaves
	if (strcmp(node_name, config_node_name) == 0)
	{
		rv = CONFIG_TRUE;
		//add child to slave
		if (subtree != NULL)
			rv = config_setting_get_slaves(subtree, ve);

		// add master node
		if (parent_node != NULL)
			xmkstring(&ve->master_name, parent_node);
			
		return rv;
	}
	
	if (subtree != NULL)
	{
		int i;
		config_setting_t * child;

		for (i = 0; (child = config_setting_get_elem(subtree, i)) != NULL; ++i)
		{
			rv = config_setting_process_tree(child, ve, config_node_name, node_name);
			if (rv != CONFIG_TRUE)
			{
				message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume layout tree.\n");
				return CONFIG_FALSE;
			}
		}
	}

	return CONFIG_TRUE;
}

static int config_setting_read_vol_layout(config_setting_t * vol_layout, volume_entry * ve, const char * node_name)
{
	config_setting_t * layout_tree = config_setting_get_member(vol_layout, "tree");
	if (layout_tree == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Missing layout information of volume %s.", ve->name.str);
		return CONFIG_FALSE;
	}

	return config_setting_process_tree(layout_tree, ve, NULL, node_name);
}

//zfs_config.this_node.node_name.str
int read_volume_layout(config_t * config, volume_entry * ve, const char * node_name);
/// reads node hierarchy from @param config to @param ve for node @param node_name
int read_volume_layout(config_t * config, volume_entry * ve, const char * node_name)
{
	config_setting_t * vol_layouts = config_lookup(config, "volume:layout");
	if (vol_layouts == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume layout from shared config, volume:layout was not found.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * vol_layout = config_setting_get_vol_layout(vol_layouts, ve->name.str);
	if (vol_layout == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "No layout was found for volume %s\n", ve->name.str);
		return CONFIG_FALSE;
	}

	return config_setting_read_vol_layout(vol_layout, ve, node_name);
}

int read_volume_list_shared_config(config_t *config, varray * volumes)
{
	config_setting_t * volume_list = config_lookup(config, "volume:list");
	if (volume_list == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume list from shared config, volume list was not found.\n");
		return CONFIG_FALSE;
	}


	unsigned int i;
	int rv = CONFIG_TRUE;
	config_setting_t * volume_setting;
	for (i = 0; rv == TRUE && (volume_setting = config_setting_get_elem(volume_list, i)) != NULL; ++i)
	{
		volume_entry ve;
		volume_entry_init(&ve);
		rv = volume_entry_read(volume_setting, &ve);
		if (rv == CONFIG_TRUE)
		{
			VARRAY_PUSH(*volumes, ve, volume_entry);
		}
		else
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume entry from config.\n");

		}
		volume_entry_destroy(&ve);

	}

	for (i = 0; i < VARRAY_USED(*volumes); ++i)
	{
		//FIXME: copy of structure volume_entry_ve
		volume_entry ve = VARRAY_ACCESS(*volumes, i, volume_entry);
		rv = read_volume_layout(config, &ve, zfs_config.this_node.node_name.str);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume hyerarchy from config.\n");
			return CONFIG_FALSE;
		}
	}

	return CONFIG_TRUE;
}

static int read_volume_tree_node_setting(config_setting_t * node_setting)
{
	int rv;
	const char * node_key;

	rv = config_setting_lookup_string(node_setting, "node", &node_key);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No node key in hierarchy tree in shared config found.\n");
		return rv;
	}

	config_setting_t * children = config_setting_get_member(node_setting, "children");
	if (children == NULL)
		return CONFIG_TRUE;

	int i;
	config_setting_t * child;
	
	for (i = 0; (child = config_setting_get_elem(children, i)) != NULL; ++i)
	{
		rv = read_volume_tree_node_setting(child);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_INFO, FACILITY_CONFIG, "Failed to read hierarchy tree from shared config.\n"); 
			return rv;
		}
	}

	return CONFIG_TRUE;
}

int read_volumes_layout_shared_config(config_t * config)
{
	config_setting_t * layouts = config_lookup(config, "volume:layout");
	if (layouts == NULL)
	{
		message(LOG_INFO, FACILITY_CONFIG, "No layout section in shared config was found.\n");
		return CONFIG_FALSE;
	}

	config_setting_t * layout;
	const char * volume_str;
	int i;

	for (i = 0; (layout = config_setting_get_elem(layouts, i)) != NULL; ++i)
	{
		int rv;

		rv = config_setting_lookup_string(layout, "volume", &volume_str);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume key from shared config.\n");
			return rv;
		}

		config_setting_t * tree = config_setting_get_member(layout, "tree");
		if (tree == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read tree key from shared config.\n");
			return rv;
		}

		rv = read_volume_tree_node_setting(tree);
		if (rv != CONFIG_TRUE)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Failed to read node hirerarchy from shared config.\n");
			return rv;
		}
		//TODO: insert slaves to selected volume
		 /* Insert slave node into slave hash table.
			 slot = htab_find_slot_with_hash(vol->slaves, nod, node_hash_name(nod), INSERT);
		*/
	}

	return CONFIG_TRUE;
}

int read_shared_config(config_t * config)
{
	int rv;

	rv = read_user_list_shared_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read user list from shared config.\n");
		return rv;
	}

	rv =  read_group_list_shared_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read group list from shared config.\n");
		return rv;
	}

	rv = read_user_mapping_shared_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read user mapping from shared config.\n");
		return rv;
	}

	rv = read_group_mapping_shared_config(config);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read group mapping from shared config.\n");
		return rv;
	}

	varray volumes;
	varray_create(&volumes, sizeof(volume_entry), 4);

	rv = read_volume_list_shared_config(config, &volumes);
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read volume list from shared config.\n");
		return rv;
	}

	varray_destroy(&volumes);

	return CONFIG_TRUE;
}
