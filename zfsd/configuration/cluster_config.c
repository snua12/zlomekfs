#include "system.h"
#include <signal.h>
#include <errno.h>
#include <libconfig.h>
#include "log.h"
#include "node.h"
#include "volume.h"
#include "user-group.h"
#include "thread.h"
#include "dir.h"
#include "node.h"
#include "volume.h"
#include "cluster_config.h"
#include "zfsd.h"
#include "network.h"

#include "config_user_group.h"
#include "configuration.h"
#include "config_volume.h"
#include "reread_config.h"

/*! Has the config reader already terminated? */
static pthread_barrier_t reading_cluster_config_barier;

/*! Invalidate configuration.  */

static void invalidate_config(void)
{
	mark_all_nodes();
	mark_all_volumes();
	mark_all_users();
	mark_all_groups();
	mark_user_mapping(NULL);
	mark_group_mapping(NULL);
	if (this_node)
	{
		mark_user_mapping(this_node);
		mark_group_mapping(this_node);
	}
}


/*! Verify configuration, fix what can be fixed. Return false if there
   remains something which can't be fixed.  */

static bool verify_config(void)
{
	if (this_node == NULL || this_node->marked)
		return false;

	destroy_marked_volumes();
	destroy_marked_nodes();

	destroy_marked_user_mapping(NULL);
	destroy_marked_group_mapping(NULL);

	destroy_marked_user_mapping(this_node);
	destroy_marked_group_mapping(this_node);

	destroy_marked_users();
	destroy_marked_groups();

	return true;
}

//TODO: varray is not good argument
static void send_reread_config_request_to_slaves(string * relative_path, uint32_t from_sid, varray v)
{

	/* First send the reread_config request to slave nodes.  */
	volume vol = volume_lookup(VOLUME_ID_CONFIG);
	if (!vol)
	{
		terminate();
		return;
	}

#ifdef ENABLE_CHECKING
	if (!vol->slaves)
		zfsd_abort();
#endif

	// add config volume slaves to vararray
	void **slot;
	node nod;
	unsigned int i;
	uint32_t sid;

	VARRAY_USED(v) = 0;
	HTAB_FOR_EACH_SLOT(vol->slaves, slot)
	{
		node nod2 = (node) * slot;

		zfsd_mutex_lock(&node_mutex);
		zfsd_mutex_lock(&nod2->mutex);
		if (nod2->id != from_sid)
			VARRAY_PUSH(v, nod2->id, uint32_t);
		zfsd_mutex_unlock(&nod2->mutex);
		zfsd_mutex_unlock(&node_mutex);
	}
	zfsd_mutex_unlock(&vol->mutex);

	// sends reread config request to slaves
	for (i = 0; i < VARRAY_USED(v); i++)
	{
		sid = VARRAY_ACCESS(v, i, uint32_t);
		nod = node_lookup(sid);
		if (nod)
			remote_reread_config(relative_path, nod);
	}
}

// reads request from config queue and process them
static void config_reader_loop(thread * t)
{
	string relative_path;
	uint32_t from_sid;

	// array for slaves
	varray v;
	varray_create(&v, sizeof(uint32_t), 4);

	/* Reread parts of configuration when notified.  */
	while (1)
	{
		/* Wait until we are notified.  */
		semaphore_down(&zfs_config.config_sem, 1);

#ifdef ENABLE_CHECKING
		if (get_thread_state(t) == THREAD_DEAD)
			zfsd_abort();
#endif
		if (get_thread_state(t) == THREAD_DYING)
			break;

		while (get_reread_config_request(&relative_path, &from_sid))
		{
			if (relative_path.str == NULL)
			{
				/* The daemon received SIGHUP, reread the local volume info.  */
				if (!reread_local_volume_info(get_local_config_path()))
				{
					terminate();
					break;
				}
				continue;
			}

			/* notify slaves by reread_config_request */
			send_reread_config_request_to_slaves(&relative_path, from_sid, v);

			/* Then reread the configuration.  */
			if (!reread_config_file(&relative_path))
			{
				terminate();
				break;
			}

			xfreestring(&relative_path);
		}
	}

	varray_destroy(&v);
}

static void cleanup_reread_config_queue(void)
{
	string relative_path;
	uint32_t from_sid;

	while (get_reread_config_request(&relative_path, &from_sid))
		if (relative_path.str != NULL)
			xfreestring(&relative_path);
}

// reads initial shared config
static bool read_shared_config()
{
	volume vol;
	dir_op_res config_dir_res;
	dir_op_res user_dir_res;
	dir_op_res group_dir_res;

	invalidate_config();

	uint32_t r;
	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "volume_root(): %s\n",
				zfs_strerror(r));
		return false;
	}

	bool rv;
	rv = read_node_list(&config_dir_res.file);
	if (rv != true)
		return rv;

	rv = read_volume_list(&config_dir_res.file);
	if (rv != true)
		return rv;

	/* Config directory may have changed so lookup it again.  */
	r = zfs_volume_root(&config_dir_res, VOLUME_ID_CONFIG);
	if (r != ZFS_OK)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "volume_root(): %s\n",
				zfs_strerror(r));
		return false;
	}

	rv = read_user_list(&config_dir_res.file);
	if (rv != true)
		return rv;

	rv = read_group_list(&config_dir_res.file);
	if (rv != true)
		return rv;

	r = zfs_extended_lookup(&user_dir_res, &config_dir_res.file, "user");
	if (r == ZFS_OK)
	{
		// read default user mapping
		if (!read_user_mapping(&user_dir_res.file, 0))
			return false;

		// read mapping for this node
		if (!read_user_mapping(&user_dir_res.file, this_node->id))
			return false;
	}

	r = zfs_extended_lookup(&group_dir_res, &config_dir_res.file, "group");
	if (r == ZFS_OK)
	{
		if (!read_group_mapping(&group_dir_res.file, 0))
			return false;

		if (!read_group_mapping(&group_dir_res.file, this_node->id))
			return false;
	}

	/* Reread the updated configuration about nodes and volumes.  */
	vol = volume_lookup(VOLUME_ID_CONFIG);
	if (!vol)
		return false;

	if (vol->master != this_node)
	{
		zfsd_mutex_unlock(&vol->mutex);

		if (!read_node_list(&config_dir_res.file))
			return false;

		if (!read_volume_list(&config_dir_res.file))
			return false;
	}
	else
		zfsd_mutex_unlock(&vol->mutex);

	if (!verify_config())
		return false;
	
	return true;
}

/*! Thread for reading a configuration.  */
static void *config_reader(void *data)
{
	thread *t = (thread *) data;
	lock_info li[MAX_LOCKED_FILE_HANDLES];

	thread_disable_signals();
	pthread_setspecific(thread_data_key, data);
	pthread_setspecific(thread_name_key, "Config reader");
	set_lock_info(li);

	bool rv = read_shared_config();
	if (rv != true)
	{
		set_thread_retval(t, ZFS_OK + 1);
		pthread_barrier_wait(&reading_cluster_config_barier);
		set_thread_state(t, THREAD_DEAD);
		return NULL;
	}

	/* Let the main thread run.  */
	set_thread_retval(t, ZFS_OK);

	pthread_barrier_wait(&reading_cluster_config_barier);

	if (get_thread_state(t) == THREAD_DYING)
	{
		set_thread_state(t, THREAD_DEAD);
		return NULL;
	}

	/* Change state to IDLE.  */
	set_thread_state(t, THREAD_IDLE);

	/* process config reread requests */
	config_reader_loop(t);

	/* Free remaining requests.  */
	cleanup_reread_config_queue();

	return NULL;
}

/*! Read global configuration of the cluster from config volume.  */
/*! Read configuration of the cluster - nodes, volumes, ... */
static bool read_global_cluster_config(void)
{
	semaphore_init(&zfs_config.config_reader_data.sem, 0);
	network_worker_init(&zfs_config.config_reader_data);
	zfs_config.config_reader_data.from_sid = 0;
	set_thread_state(&zfs_config.config_reader_data, THREAD_BUSY);

	pthread_barrier_init(&reading_cluster_config_barier, NULL, 2);

	if (pthread_create(&zfs_config.config_reader_data.thread_id, NULL, config_reader,
					   &zfs_config.config_reader_data))
	{
		message(LOG_CRIT, FACILITY_CONFIG, "pthread_create() failed\n");
		set_thread_state(&zfs_config.config_reader_data, THREAD_DEAD);
		zfs_config.config_reader_data.thread_id = 0;

		network_worker_cleanup(&zfs_config.config_reader_data);
		semaphore_destroy(&zfs_config.config_reader_data.sem);
		return false;
	}

	pthread_barrier_wait(&reading_cluster_config_barier);
	pthread_barrier_destroy(&reading_cluster_config_barier);

	return get_thread_retval(&zfs_config.config_reader_data) == ZFS_OK;
}

bool read_cluster_config(void)
{
	if (!init_config_volume())
	{
		message(LOG_CRIT, FACILITY_CONFIG, "Could not init config volume\n");
		return false;
	}

	if (!read_global_cluster_config())
	{
		message(LOG_CRIT, FACILITY_CONFIG,
				"Could not read global configuration\n");
		return false;
	}

	return true;
}
