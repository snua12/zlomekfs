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
#include <unistd.h>
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

/* Regulator of server pool.  */
static pthread_t server_regulator;

/* Local function prototypes.  */
static int server_thread_create ();
static void *server_worker (void *data);
static void server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp);
static void * server_pool_regulator (void *data);

/* Function which receives a RPC request and passes it to some server thread.
   it also regulates the number of server threads.  */

static void
server_dispatch (struct svc_req *rqstp, register SVCXPRT *transp)
{
  size_t index;

  pthread_mutex_lock (&server_pool.idle.mutex);

  /* Regulate the number of threads.  */
  thread_pool_regulate (&server_pool, server_thread_create);

  /* Select an idle thread and forward the request to it.  */
  index = queue_get (&server_pool.idle);
  server_pool.threads[index].t.state = THREAD_BUSY;
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
      if (t->state == THREAD_BUSY)
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
	queue_put (&server_pool.idle, t->index);
      else
	{
#ifdef ENABLE_CHECKING
	  if (t->state != THREAD_DYING)
	    abort ();
#endif
	  pthread_mutex_unlock (&server_pool.idle.mutex);
	  return data;
	}
      pthread_mutex_unlock (&server_pool.idle.mutex);
    }
}

/* Create a new server thread.  It expects SERVER_POLL.EMPTY.MUTEX and
   SERVER_POOL.IDLE.MUTEX to be locked.  */

static int
server_thread_create ()
{
  size_t index = queue_get (&server_pool.empty);
  thread *t = &server_pool.threads[index].t;
  int r;

#ifdef ENABLE_CHECKING
  if (pthread_mutex_trylock (&server_pool.idle.mutex) == 0)
    abort ();
  if (pthread_mutex_trylock (&server_pool.empty.mutex) == 0)
    abort ();
#endif

  pthread_mutex_init (&t->mutex, NULL);
  pthread_mutex_lock (&t->mutex);
  t->state = THREAD_IDLE;
  r = pthread_create (&t->thread_id, NULL, server_worker, t);
  if (r == 0)
    {
      queue_put (&server_pool.idle, index);
    }
  else
    {
      pthread_mutex_unlock (&t->mutex);
      pthread_mutex_destroy (&t->mutex);
      t->state = THREAD_DEAD;
      queue_put (&server_pool.empty, index);
      message (-1, stderr, "pthread_create() failed\n");
    }

  return r;
}

/* Main function of thread which periodically regulates the number of
   threads in the SERVER_POOL.  */

static void *
server_pool_regulator (void *data)
{
  while (1)
    {
      pthread_mutex_lock (&server_pool.idle.mutex);
      thread_pool_regulate (&server_pool, server_thread_create);
      pthread_mutex_unlock (&server_pool.idle.mutex);
      sleep (60);	/* FIXME: Read the number from configuration.  */
    }
}

void
create_server_pool ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&server_pool, 1024, 4, 16);

  pthread_mutex_lock (&server_pool.idle.mutex);
  pthread_mutex_lock (&server_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      server_thread_create ();
    }
  pthread_mutex_unlock (&server_pool.empty.mutex);
  pthread_mutex_unlock (&server_pool.idle.mutex);
  if (pthread_create (&server_regulator, NULL, server_pool_regulator, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
    }
}

/* Register and run the ZFS protocol server.  This function never returns
   (unless error occurs).  */

void
register_server ()
{
  SVCXPRT *transp;

  pmap_unset (ZFS_PROGRAM, ZFS_VERSION);

  transp = svcudp_create (RPC_ANYSOCK);
  if (transp == NULL)
    {
      message (-1, stderr, "cannot create udp service.\n");
      return;
    }
  if (!svc_register (transp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_UDP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, udp).",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

  transp = svctcp_create (RPC_ANYSOCK, 0, 0);
  if (transp == NULL)
    {
      message (-1, stderr, "cannot create tcp service.\n");
      return;
    }
  if (!svc_register (transp, ZFS_PROGRAM, ZFS_VERSION, server_dispatch,
		     IPPROTO_TCP))
    {
      message (-1, stderr,
	       "unable to register (ZFS_PROGRAM<%d>, ZFS_VERSION<%d>, tcp).",
	       ZFS_PROGRAM, ZFS_VERSION);
      return;
    }

  svc_run ();
  message (-1, stderr, "svc_run returned.\n");
}
