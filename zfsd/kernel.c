/* Client thread functions.
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
#include <signal.h>
#include "constant.h"
#include "client.h"
#include "log.h"
#include "memory.h"
#include "thread.h"

/* Pool of client threads.  */
static thread_pool client_pool;

/* Data for client pool regulator.  */
static thread_pool_regulator_data client_regulator_data;

/* Local function prototypes.  */
static void *client_worker (void *data);
#if 0
static void client_dispatch (...);
#endif
static void client_worker_init (thread *t);
static void client_worker_cleanup (void *data);

#if 0
/* Function which receives a request and passes it to some client thread.
   It also regulates the number of client threads.  */

static void
client_dispatch ()
{
  size_t index;

  pthread_mutex_lock (&client_pool.idle.mutex);

  /* Regulate the number of threads.  */
  thread_pool_regulate (&client_pool, client_worker, client_worker_init);

  /* Select an idle thread and forward the request to it.  */
  index = queue_get (&client_pool.idle);
#ifdef ENABLE_CHECKING
  if (client_pool.threads[index].t.state == THREAD_BUSY)
    abort ();
#endif
  client_pool.threads[index].t.state = THREAD_BUSY;
  /* FIXME: read and pass request */
  pthread_mutex_unlock (&client_pool.threads[index].t.mutex);

  pthread_mutex_unlock (&client_pool.idle.mutex);
}
#endif

/* Initialize client thread T.  */

static void
client_worker_init (thread *t)
{
  t->u.client.buffer = (char *) xmalloc (ZFS_MAX_REQUEST_LEN);
}

/* Cleanup client thread DATA.  */

static void
client_worker_cleanup (void *data)
{
  thread *t = (thread *) data;

  free (t->u.client.buffer);
}

/* The main function of the client thread DATA.  */

static void *
client_worker (void *data)
{
  thread *t = (thread *) data;

  pthread_cleanup_push (client_worker_cleanup, data);

  while (1)
    {
      /* Wait until client_dispatch wakes us up.  */
      pthread_mutex_lock (&t->mutex);

#ifdef ENABLE_CHECKING
      if (t->state == THREAD_DEAD)
	abort ();
#endif

      /* We were requested to die.  */
      if (t->state == THREAD_DYING)
	break;

      /* We have some work to do.  */
      /* FIXME: TODO: call appropriate routine */

      /* Put self to the idle queue if not requested to die meanwhile.  */
      pthread_mutex_lock (&client_pool.idle.mutex);
      if (t->state == THREAD_BUSY)
	{
	  queue_put (&client_pool.idle, t->index);
	  t->state = THREAD_IDLE;
	}
      else
	{
#ifdef ENABLE_CHECKING
	  if (t->state != THREAD_DYING)
	    abort ();
#endif
	  pthread_mutex_unlock (&client_pool.idle.mutex);
	  break;
	}
      pthread_mutex_unlock (&client_pool.idle.mutex);
    }

  pthread_cleanup_pop (1);

  return data;
}

/* Create client threads and related threads.  */ 

void
create_client_threads ()
{
  int i;

  /* FIXME: read the numbers from configuration.  */
  thread_pool_create (&client_pool, 64, 4, 16);

  pthread_mutex_lock (&client_pool.idle.mutex);
  pthread_mutex_lock (&client_pool.empty.mutex);
  for (i = 0; i < /* FIXME: */ 10; i++)
    {
      create_idle_thread (&client_pool, client_worker, client_worker_init);
    }
  pthread_mutex_unlock (&client_pool.empty.mutex);
  pthread_mutex_unlock (&client_pool.idle.mutex);

  thread_pool_create_regulator (&client_regulator_data, &client_pool,
				client_worker, client_worker_init);
}

/* Make the connection with kernel.  */

int
initialize_client ()
{
  return 1;
}

/* Terminate client threads and destroy data structures.  */

void
client_cleanup ()
{
  /* TODO: for each thread waiting for reply do:
     set retval to indicate we are exiting
     unlock the thread	*/

  pthread_kill (client_regulator_data.thread_id, SIGUSR1);
  thread_pool_destroy (&client_pool);
}
