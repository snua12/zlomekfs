/*! \file \brief Functions for managing thread pools.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef THREAD_H
#define THREAD_H

#include "system.h"
#include <inttypes.h>
#include <stddef.h>
#include "pthread-wrapper.h"
#include "queue.h"
#include "semaphore.h"
#include "fibheap.h"
#include "data-coding.h"
#include "zfs-prot.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! Key for thread specific data.  */
extern pthread_key_t thread_data_key;

/*! Key for thread name.  */
extern pthread_key_t thread_name_key;

/*! Limits for number of threads.  */
typedef struct thread_limit_def
{
	size_t max_total;			/*!< maximal number of total threads */
	size_t min_spare;			/*!< minimal number of spare threads */
	size_t max_spare;			/*!< maximal number of spare threads */
} thread_limit;

/*! State of the thread.  */
typedef enum thread_state_def
{
	THREAD_DEAD,				/*!< thread is not created */
	THREAD_DYING,				/*!< thread is dying */
	THREAD_IDLE,				/*!< thread is idle */
	THREAD_BUSY					/*!< thread is working */
} thread_state;

struct fd_data_def;

/*! \brief Additional data for a network thread.  */
typedef struct network_thread_data_def
{
	DC *dc;						/*!< buffer for request to this node */
	call_args args;				/*!< union for decoded call arguments */
	direction dir;				/*!< method of the request */
	struct fd_data_def *fd_data;	/*!< passed from main network thread */
	unsigned int generation;	/*!< generation of file descriptor */
	unsigned int index;			/*!< index of FD in array "active" */
} network_thread_data;

/*! \brief Additional data for a kernel thread.  */
typedef struct kernel_thread_data_def
{
	void *buf;					/*!< buffer for request to this node */
	size_t buf_size;
	struct fuse_chan *fuse_ch;
} kernel_thread_data;

/*! \brief Additional data for an update thread.  */
typedef struct update_thread_data_def
{
	/*! File handle to update.  */
	zfs_fh fh;
	/*! Thread is a slow updater. */
	bool slow;
} update_thread_data;

/*! \brief Definition of thread's variables.  */
typedef struct thread_def
{
	/*! Mutex protecting the state of thread.  */
	pthread_mutex_t mutex;

	/*! State of the thread.  */
	thread_state state;

	/*! The sequential number of the thread.  */
	size_t index;

	/*! The ID of the thread which is set by pthread_create.  */
	pthread_t thread_id;

	/*! Semaphore used to stop an idle thread.  */
	semaphore sem;

	DC *dc_call;				/*!< buffer for request for remote node */
	DC *dc_reply;				/*!< buffer for reply from remote node */
	int32_t retval;				/*!< return value for request.  */
	uint32_t from_sid;			/*!< request came from node SID */

	/*! Additional data for each subtype.  */
	union
	{
		network_thread_data network;
		kernel_thread_data kernel;
		update_thread_data update;
	} u;						// FIXME: none or meaningfull name
} thread;

/*! \brief Thread datatype padded to 256 bytes to avoid cache ping pong.  */
typedef union padded_thread_def
{
	thread t;
	char padding[((sizeof(thread) + 255) / 256) * 256];
}
padded_thread;

/*! Type of a routine started in a new thread.  */
typedef void *(*thread_start) (void *);

/*! Type of thread initializer.  */
typedef void (*thread_init) (thread *);

/*! \brief Thread pool.  */
typedef struct thread_pool_def
{
	volatile bool terminate;	/*!< shall threads in this pool terminate? */

	size_t min_spare_threads;	/*!< minimal number of spare threads */
	size_t max_spare_threads;	/*!< maximal number of spare threads */
	size_t size;				/*!< total number of slots for threads */
	void *unaligned_array;		/*!< pointer returned by xmalloc */
	padded_thread *threads;		/*!< thread slots, previous pointer aligned */
	pthread_mutex_t mutex;		/*!< mutex for queues */
	queue idle;					/*!< queue of idle threads */
	queue empty;				/*!< queue of empty thread slots */
	thread_start worker_start;	/*!< start routine of the worker thread */
	thread_init worker_init;	/*!< initialization routine for worker thread 
								 */

	/* Data for main thread.  */
	volatile pthread_t main_thread;	/*!< thread ID of the main thread */
	pthread_mutex_t main_in_syscall;	/*!< main thread is in blocking
										   syscall */

	/* Data for thread pool regulator.  */
	volatile pthread_t regulator_thread;	/*!< thread ID of the regulator */
	pthread_mutex_t regulator_in_syscall;	/*!< regulator is in blocking
											   syscall */
} thread_pool;

/*! \brief Description of thread waiting for reply.  */
typedef struct waiting4reply_data_def
{
	uint32_t request_id;
	thread *t;
	fibnode node;
} waiting4reply_data;


/*! Verify whether the thread limits are valid. \param limit Thread limits.
   \param name Name of the threads.  */
extern bool is_valid_thread_limit(thread_limit * limit, const char *name);

/*! Get value of RUNNING flag.  */
extern bool keep_running(void);


/*! set value of RUNNING flag.  */
extern void set_running(bool value);

extern bool thread_pool_terminate_p(thread_pool * pool);
extern void thread_terminate_blocking_syscall(volatile pthread_t * thid,
											  pthread_mutex_t * mutex);
extern int wait_for_thread_to_die(volatile pthread_t * thid, void **ret);
extern thread_state get_thread_state(thread * t);
extern void set_thread_state(thread * t, thread_state state);

void set_thread_retval(thread * t, int32_t retval);
int32_t get_thread_retval(thread * t);

extern bool thread_pool_create(thread_pool * pool, thread_limit * limit,
							   thread_start main_start,
							   thread_start worker_start,
							   thread_init worker_init);
extern void thread_pool_terminate(thread_pool * pool);
extern void thread_pool_destroy(thread_pool * pool);
extern int create_idle_thread(thread_pool * pool);
extern int destroy_idle_thread(thread_pool * pool);
extern void thread_disable_signals(void);
extern void thread_pool_regulate(thread_pool * pool);

#ifdef __cplusplus
}
#endif

#endif
