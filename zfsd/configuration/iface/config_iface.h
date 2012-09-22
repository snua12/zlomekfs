/**
 *  \file config_iface.h
 * 
 *  \brief
 *  Interface between configuratinon and others parts of ZlomkeFS
 *  \author Josef Zlomek
 *  \author Ales Snuparek
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


#ifndef CONFIG_IFACE_H
#define CONFIG_IFACE_H

/*! \brief Structure for storing informations about volumes from shared config. */
typedef struct volume_entry_def
{
	uint32_t id; /**< ID of volume */
	string name; /**< name of volume */
	string mountpoint; /**< mountpoint of volume */
	string master_name; /**< name of master node */
	varray slave_names; /** names of slave nodes */
}
volume_entry;

/*! \brief node and zfs user tuple */
typedef struct user_mapping_def
{
	string zfs_user; /**< global user name */
	string node_user; /**< local user name */
}
user_mapping;

/*! \brief local and zfs group tuple */
typedef struct group_mapping_def
{
	string zfs_group; /**< global group name */
	string node_group; /**< local group name */
}
group_mapping;

#endif
