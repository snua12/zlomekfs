/*! \file
    \brief Configuration.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef CONFIG_H
#define CONFIG_H

#include "system.h"
#include <stdio.h>
#include <inttypes.h>
#include "pthread.h"
#include "memory.h"
#include "semaphore.h"
#include "thread.h"
#include "fh.h"

/*! Data for config reader thread.  */
extern thread config_reader_data;

/*! Semaphore for managing the reread request queue.  */
extern semaphore config_sem;

/*! File used to communicate with kernel.  */
extern string kernel_file_name;

/*! Node which the local node should fetch the global configuration from.  */
extern char *config_node;

extern void set_default_uid_gid (void);
extern void add_reread_config_request_dentry (internal_dentry dentry);
extern void add_reread_config_request_local_path (volume vol, string *path);
extern void add_reread_config_request (string *relative_path,
				       uint32_t from_sid);
extern bool read_cluster_config (void);
extern bool read_config_file (const char *file);

extern void initialize_config_c (void);
extern void cleanup_config_c (void);

#endif
