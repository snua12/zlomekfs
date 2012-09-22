/**
 *  \file config_volume.c
 * 
 *  \brief Implements fuction for reading volume config
 *  \author Ales Snuparek (refactoring and partial rewrite and libconfig integration)
 *  \author Josef Zlomek (initial experimental implementation)
 *
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include "config_volume.h"
#include "configuration.h"
#include "config_iface.h"
#include "dir.h"
#include "zfsd.h"
#include "zfsio.h"
#include "libconfig.h"
#include "shared_config.h"
#include "pthread-wrapper.h"

/*! \brief add slaves nodes to volume
 *  \return true on success
 *  \return false on fail*/
static bool_t volume_set_slave_info(volume vol, varray * slave_names)
{
	unsigned int i;

	htab_empty(vol->slaves);
	for (i = 0; i < VARRAY_USED(*slave_names); ++i)
	{
		char * slave_name = VARRAY_ACCESS(*slave_names, i, char *);

		string str;
		str.str =  slave_name;
		str.len = strlen(slave_name);
		node nod = node_lookup_name(&str);
		if (nod == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Slave node \"%s\" was not found.\n", slave_name);
			continue;
		}

		/* Insert slave node into slave hash table. */
		void ** slot = htab_find_slot_with_hash(vol->slaves, nod, node_hash_name(nod),
										INSERT);
		*slot = nod;
		zfsd_mutex_unlock(&nod->mutex);
	}

	return true;
}

/*! \brief updates volume by array of volume_entry
 *  \return true on success
 *  \return false on fail*/
static bool_t update_volumes(varray * volumes)
{
	zfsd_mutex_lock(&fh_mutex);
	zfsd_mutex_lock(&volume_mutex);

	unsigned int i;
	for (i = 0; i < VARRAY_USED(*volumes); ++i)
	{
		//FIXME: copy of structure volume_entry_ve
		volume_entry ve = VARRAY_ACCESS(*volumes, i, volume_entry);
		volume vol = volume_lookup_nolock(ve.id);
		if (vol == NULL)
		{
			message(LOG_ERROR, FACILITY_CONFIG, "Volume with id %d not found, please add it to local config.\n", ve.id);
			continue;
		}

		node master_node;
		if (stringlen(&ve.master_name))
		{
			master_node = node_lookup_name(&ve.master_name);
			if (master_node == NULL)
			{
				message(LOG_ERROR, FACILITY_CONFIG, "Master node name \"%s\" is invaled\n", ve.master_name.str);
				zfsd_mutex_unlock(&vol->mutex);
				continue;
			}
		}
		else
		{
			master_node = this_node;
			zfsd_mutex_lock(&this_node->mutex);
		}

		volume_set_common_info(vol, &ve.name, &ve.mountpoint, master_node);
		zfsd_mutex_unlock(&master_node->mutex);

		if (vol->slaves != NULL)
			volume_set_slave_info(vol, &ve.slave_names);

		zfsd_mutex_unlock(&vol->mutex);

	}

	zfsd_mutex_unlock(&volume_mutex);
	zfsd_mutex_unlock(&fh_mutex);

	return true;

}

/*! \brief Read list of volumes from CONFIG_DIR/volume_list.
 *  \return true on success
 *  \return false on fail*/
bool read_volume_list(zfs_fh * config_dir)
{
	dir_op_res volume_list_res;
	int32_t r;

	r = zfs_extended_lookup(&volume_list_res, config_dir, "volume_list");
	if (r != ZFS_OK)
		return false;

	zfs_file * file = zfs_fopen(&volume_list_res.file);
	if (file == NULL)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to read shared user list.\n");
		return false;
	}

	config_t config;
	config_init(&config);
	int rv;
	rv =  config_read(&config, zfs_fdget(file));
	if (rv != CONFIG_TRUE)
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to parse shared user list.\n");
		zfs_fclose(file);
		return false;
	}

	varray volumes;
	varray_create(&volumes, sizeof(volume_entry), 4);

	rv = read_volume_list_shared_config(&config, &volumes);
	if (rv == CONFIG_TRUE)
	{
		rv = update_volumes(&volumes);
		
	}
	else
	{
		message(LOG_ERROR, FACILITY_CONFIG, "Failed to process shared user list.\n");
	}

	config_destroy(&config);
	zfs_fclose(file);
	varray_destroy(&volumes);

	return (rv == CONFIG_TRUE);
}
