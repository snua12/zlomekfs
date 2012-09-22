/**
 *  \file config_user_group.h
 * 
 *  \brief Implements fuction for reading user and group config
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

#ifndef CONFIG_USER_GROUP_H
#define CONFIG_USER_GROUP_H

#include "system.h"
#include "fh.h"

#ifdef __cplusplus
extern "C" 
{
#endif

/*! \brief Read list of users from CONFIG_DIR/user_list.
 *  \return true on success
 *  \return false on fail*/
extern bool read_user_list(zfs_fh * config_dir);

/*! \brief Read list of groups from CONFIG_DIR/group_list.
 *  \return true on success
 *  \return false on fail*/
extern bool read_group_list(zfs_fh * config_dir);

/*! \brief Read list of user mapping.  If SID == 0 read the default user mapping
 *  from CONFIG_DIR/user/default else read the special mapping for node SID.
 *  \return true on success
 *  \return false on fail*/
extern bool read_user_mapping(zfs_fh * user_dir, uint32_t sid);

/*! \brief Read list of group mapping.  If SID == 0 read the default group mapping
 *  from CONFIG_DIR/group/default else read the special mapping for node SID. 
 *  \return true on success
 *  \return false on fail*/
bool read_group_mapping(zfs_fh * group_dir, uint32_t sid);

#ifdef __cplusplus
}
#endif

#endif
