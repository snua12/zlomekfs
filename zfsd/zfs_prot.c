/* ZFS protocol.
   Copyright (C) 2003 Josef Zlomek

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

#include "system.h"
#include <pthread.h>
#include "zfs_prot.h"
#include "data-coding.h"
#include "thread.h"
#include "server.h"

/* FIXME: These are some temporary dummy functions to make linker happy.  */
#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_server (ARGS_TYPE *args, DC *dc)			\
{									\
  /* return zfs_##FUNCTION## (args, dc); */				\
  return ZFS_OK;							\
}
#include "zfs_prot.def"
#undef DEFINE_ZFS_PROC

#define DEFINE_ZFS_PROC(NUMBER, NAME, FUNCTION, ARGS_TYPE)		\
int									\
zfs_proc_##FUNCTION##_client (ARGS_TYPE *args, node *nod)		\
{									\
  thread *t = (thread *) pthread_getspecific (server_thread_key);	\
  server_thread_data *td = &t->u.server;				\
									\
  if (!encode_##ARGS_TYPE (&td->dc, args))				\
    return ZFS_REQUEST_TOO_LONG;					\
									\
  send_request (d, nod);						\
									\
  return d->retval;							\
}
