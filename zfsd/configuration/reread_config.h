/**
 *  \file reread_config.h 
 * 
 *  \brief Implements fuction for reread cluster config.
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

#ifndef REREAD_CONFIG_H
#define REREAD_CONFIG_H

#include "system.h"
#include "fh.h"
#include "memory.h"

typedef struct reread_config_request_def *reread_config_request;
/*! \brief Element of list of requests for config reread.  */
struct reread_config_request_def
{
	/*! Next element in the chain.  */
	reread_config_request next;

	/*! Path relative to root of config volume.  */
	string relative_path;

	/*! Node which the request came from.  */
	uint32_t from_sid;
};

/*! \brief Add request to reread config file PATH on volume VOL to queue.
 *  \return true on success
 *  \return false on fail*/
extern void add_reread_config_request_local_path(volume vol, string * path);

/*! \brief Add request to reread config file DENTRY to queue.
 *  \return true on success
 *  \return false on fail*/
extern void add_reread_config_request_dentry(internal_dentry dentry);

/*! \brief Add a request to reread config into queue
 *  \return true on success
 *  \return false on fail*/
extern void add_reread_config_request(string * relative_path, uint32_t from_sid);

/*! \brief Reread configuration file RELATIVE_PATH.
 *  \return true on success
 *  \return false on fail*/
extern bool reread_config_file(string * relative_path);

/*! \brief Reread local info about volumes.
 *  \param path Path where local configuration is stored.
 *  \return true on success
 *  \return false on fail*/
extern bool reread_local_volume_info(const char * path);

/*! \brief Get a request to reread config from queue
 *   and store the relative path of
 *   the file to be reread to RELATIVE_PATH and the node ID which the request
 *   came from to FROM_SID.
 *  \return true on success
 *  \return false on fail*/
extern bool get_reread_config_request(string * relative_path, uint32_t * from_sid);

/*! \brief Read list of nodes from CONFIG_DIR/node_list.
 *  \return true on success
 *  \return false on fail*/
extern bool read_node_list(zfs_fh * config_dir);

#endif
