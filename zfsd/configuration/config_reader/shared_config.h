/**
 *  \file shared_config.h
 * 
 *  \brief Implements cluster config readers
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

#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <libconfig.h>
#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief callback function for add mapping */
typedef void (*add_mapping)(void * data, uint32_t id, string * name);

/*! \brief callback function for add pair mapping */
typedef void (*add_pair_mapping)(void * data, const char * local, const char * remote);

/*! \brief read node list shared config
 *  \return same type as functions from libconfig */
int read_node_list_shared_config(config_t * config);

/*! \brief read mapping 
 *  \return same type as functions from libconfig */
int read_mapping_setting(config_setting_t * setting, add_mapping add, void * data);

/*! \brief read user list shared config
 *  \return same type as functions from libconfig */
int read_user_list_shared_config(config_t * config);

/*! \brief read group list shared config
 *  \return same type as functions from libconfig */
int read_group_list_shared_config(config_t * config);

/*! \brief read user mapping shared config
 *  \return same type as functions from libconfig */
int read_user_mapping_shared_config(config_t * config, const char * node_name, void * data);

/*! \brief read group mapping shared config
 *  \return same type as functions from libconfig */
int read_group_mapping_shared_config(config_t * config, const char * node_name, void * data);

/*! \brief read volume list shared config
 *  \return same type as functions from libconfig */
int read_volume_list_shared_config(config_t *config, varray * volumes);

/*! \brief read volume layout shared config
 *  \return same type as functions from libconfig */
int read_volumes_layout_shared_config(config_t * config);

/*! \brief read shared config
 *  \return same type as functions from libconfig */
int read_shared_config(config_t * config);

#ifdef __cplusplus
}
#endif

#endif
