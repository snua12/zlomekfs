/**
 *  \file local_config.h
 * 
 *  \brief Implements local config readers
 *  \author Ales Snuparek 
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

#ifndef LOCAL_CONFIG_H
#define LOCAL_CONFIG_H

#include <libconfig.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// reads volumes config from local config
int read_volumes_local_config(config_t * config, bool reread);

/// reads volumes config from local config file
int read_volumes_local_config_from_file(const char * local_path, bool reread);

/// reads users local cofnig
int read_users_local_config(config_t * config);

/// reads group local config
int read_groups_local_config(config_t * config);

/// reads information about this node from local config
int read_this_node_local_config(config_t * config);

/// reads information about config node from local config
int read_config_node_local_config(config_t * config);

/// reads system specific config from local config
int read_system_specific_config(config_t * config);

/// reads thread limits from local config
int read_threads_config(config_t * config);

#ifdef ENABLE_VERSIONS
/// reads versioning config
int read_versioning_config(config_t * config);
#endif

/// reads local configuration
int read_local_config(config_t * config);

/// reads local config from selected file
int read_local_config_from_file(const char * local_path);

#ifdef __cplusplus
}
#endif

#endif

