/* Server thread functions.
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
#include <stdlib.h>
#include <pthread.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "server.h"
#include "log.h"
#include "thread.h"
#include "zfs_prot.h"

extern void zfs_program_1 (struct svc_req *rqstp, SVCXPRT *transp);

/* Pool of server threads.  */
static thread_pool server_pool;

/* Data for server pool regulator.  */
static thread_pool_regulator_data server_regulator_data;

/* Local function prototypes.  */
static void *server_worker (void *data);
static void server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp);

/* Function which receives a RPC request and passes it to some server thread.
   It also regulates the number of server threads.  */

static void
server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp)
{
  size_t index;

  pthread_mutex_lock (&server_pool.idle.mutex);

  /* Regulate the number of threads.  */
  thread_pool_regulate (&server_pool, server_worker, NULL);

  /* Select an idle thread and forward the request to it.  */
  index = queue_get (&server_pool.idle);
#ifdef ENABLE_CHECKING
  if (server_pool.threads[index].t.state == THREAD_BUSY)
    abort ();
#endif
  server_pool.threads[index].t.state = THREAD_BUSY;
  server_pool.threads[index].t.u.server.rqstp = rqstp;
  server_pool.threads[index].t.u.server.transp = transp;
  printf ("%p %p\n", rqstp, transp);
  pthread_mutex_unlock (&server_pool.threads[index].t.mutex);

  pthread_mutex_unlock (&server_pool.idle.mutex);
}

/* The main function of the server thread.  */

static void *
server_worker (void *data)
{
  thread *t = (thread *) data;

  while (1)
    {
      /* Wait until server_dispatch wakes us up.  */
      pthread_mutex_lock (&t->mutex);

#ifdef ENABLE_CHECKING
      if (t->state == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (t->state == THREAD_DYING)
	return data;

      /* We have some work to do.  */
      zfs_program_1(t->u.server.rqstp, t->u.server.transp);

      /* Put self to the idle queue if not requested to die meanwhile.  */
      pthread_mutex_lock (&server_pool.idle.mutex);
      if (t->state == THREAD_BUSY)
	{
	  queue_put (&server_pool.idle, t->index);
	  t->state = THREAD_IDLE;
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (t->state != THREAD_DYING)
	    abort ();
#endif
	  pthread_mutex_unlock (&server_pool.idle.mutex);
	  break;
	}
      pthread_mutex_unlock (&server_pool.idle.mutex);
    }

  return data;
}

/* Create server threads and related threads.  */

void
create_server_threads ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&server_pool, 256, 4, 16);

  pthread_mutex_lock (&server_pool.idle.mutex);
  pthread_mutex_lock (&server_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      create_idle_thread (&server_pool, server_worker, NULL);
    }
  pthread_mutex_unlock (&server_pool.empty.mutex);
  pthread_mutex_unlock (&server_pool.idle.mutex);

  thread_pool_create_regulator (&server_regulator_data, &server_pool,
				server_worker, NULL);
}

/* Register and run the ZFS protocol server.  This function never returns
   (unless error occurs).  */

void
register_server ()
{
  SVCXPRT *udp;
  SVCXPRT *tcp;

  pmap_unset (ZFS_PROGRAM, ZFS_VERSION);

  udp = svcudp_create (RPC_ANYSOCK);
  if (udp == NULL)
    {
      message (-1, stderr, "cannot create udp service.\n");
      return;
    }
  if (!svc_register (udp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_UDP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, udp).\n",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

  tcp = svctcp_create (RPC_ANYSOCK, 0, 0);
  if (tcp == NULL)
    {
      message (-1, stderr, "cannot create tcp service.\n");
      return;
    }
  if (!svc_register (tcp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_TCP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, tcp).\n",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

#if 0
  svc_run ();
#endif
  message (-1, stderr, "svc_run returned.\n");

#if 0
  /* This does not help :-( */
  svc_destroy (udp);
  svc_destroy (tcp);
  svc_unregister (ZFS_PROGRAM, ZFS_VERSION);
#endif
}
