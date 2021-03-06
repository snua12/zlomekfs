/*! 
 * \file zfsd.c \brief ZFS daemon main implementation.  
 * \author Ales Snuparek
 * \author Josef Zlomek
 */

/* 
 * Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak
 * 
 * This file is part of ZFS.
 * 
 * ZFS is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 * 
 * ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with ZFS; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
 * download it from http://www.gnu.org/licenses/gpl.html 
 */

#include "system.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>

#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#include <libconfig.h>
#include "pthread-wrapper.h"
#include "zfsd.h"
#include "memory.h"
#include "semaphore.h"
#include "hardlink-list.h"
#include "journal.h"
#include "reread_config.h"
#include "zfs_config.h"
#include "configuration.h"
#include "local_config.h"
#include "thread.h"

#if defined ENABLE_FS_INTERFACE || defined ENABLE_HTTP_INTERFACE
#include "fs-iface.h"
#endif

#include "network.h"
#include "random.h"
#include "queue.h"
#include "fh.h"
#include "cap.h"
#include "file.h"
#include "node.h"
#include "volume.h"
#include "zfs-prot.h"
#include "metadata.h"
#include "user-group.h"
#include "update.h"
#include "log.h"
#include "control.h"
#include "zfsd_state.h"
#include "zfsd_args.h"

/* 
 *! Thread ID of the main thread.  
 */
pthread_t main_thread;


#ifndef SI_FROMKERNEL
#define SI_FROMKERNEL(siptr)	((siptr)->si_code > 0)
#endif

/* 
 *! Signal handler for terminating zfsd.  
 */
static void exit_sighandler(ATTRIBUTE_UNUSED int signum)
{
	message(LOG_NOTICE, FACILITY_ZFSD, "Entering exit_sighandler\n");

	set_running(false);

#ifdef ENABLE_FS_INTERFACE 
#if defined HAVE_FUSE
	thread_pool_terminate(&kernel_pool);
#elif defined HAVE_DOKAN
#endif
#endif

	thread_pool_terminate(&network_pool);

	if (update_pool.regulator_thread)
	{
		queue_exiting(&update_queue);
		thread_pool_terminate(&update_pool);
	}

	thread_terminate_blocking_syscall(&cleanup_dentry_thread, &cleanup_dentry_thread_in_syscall);

	if (zfs_config.config_reader_data.thread_id)
	{
		set_thread_state(&zfs_config.config_reader_data, THREAD_DYING);
		semaphore_up(&zfs_config.config_sem, 1);
	}

	/* 
	 * Terminate the sleep.  
	 */
	pthread_kill(main_thread, SIGUSR1);

	message(LOG_NOTICE, FACILITY_ZFSD, "Leaving exit_sighandler\n");
}

/* 
 *! Report the fatal signal.  
 */
static void fatal_sigaction(int signum, siginfo_t * info,
							ATTRIBUTE_UNUSED void *data)
{
	/* 
	 * Process only signals which are from kernel.  
	 */
	if (SI_FROMKERNEL(info))
	{
		switch (signum)
		{
		case SIGBUS:
		case SIGSEGV:
			{
#ifdef HAVE_UCONTEXT_H
#ifdef __i386__
				ucontext_t *context = (ucontext_t *) data;

				internal_error("%s at %p when accessing %p",
							   strsignal(signum),
							   context->uc_mcontext.gregs[REG_EIP],
							   info->si_addr);
#elif defined __x86_64__
				ucontext_t *context = (ucontext_t *) data;

				internal_error("%s at %p when accessing %p",
							   strsignal(signum),
							   context->uc_mcontext.gregs[REG_RIP],
							   info->si_addr);
#endif
#endif
				internal_error("%s when accessing %p", strsignal(signum),
							   info->si_addr);
			}
			break;

		case SIGILL:
		case SIGFPE:
			internal_error("%s at %p", strsignal(signum), info->si_addr);
			break;

		default:
			internal_error("%s", strsignal(signum));
			break;
		}
	}
}

/* 
 *! Signal handler for SIGHUP. \param signum Number of the received
 * signal.  
 */
static void hup_sighandler(ATTRIBUTE_UNUSED int signum)
{
	//update loval volume list see reread_local_volume_info
	add_reread_config_request(&invalid_string, 0);
}

/* 
 *! Empty signal handler, used to break poll and other syscalls.  
 */
static void dummy_sighandler(ATTRIBUTE_UNUSED int signum)
{
	message(LOG_INFO, FACILITY_ZFSD, "signalled %lu\n", pthread_self());
}

/* 
 *! Initialize signal handlers.  
 */
static void init_sig_handlers(void)
{
	struct sigaction sig;

	/* 
	 * Remember the thread ID of this thread.  
	 */
	main_thread = pthread_self();

	/* 
	 * Set the signal handler for terminating zfsd.  
	 */
	sigfillset(&sig.sa_mask);
	sig.sa_handler = exit_sighandler;
	sig.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sig, NULL);
	sigaction(SIGQUIT, &sig, NULL);
	sigaction(SIGTERM, &sig, NULL);

	/* 
	 * Set the signal handler for fatal errors.  
	 */
	sigfillset(&sig.sa_mask);
	sig.sa_sigaction = fatal_sigaction;
	sig.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGILL, &sig, NULL);
	sigaction(SIGBUS, &sig, NULL);
	sigaction(SIGFPE, &sig, NULL);
	sigaction(SIGTRAP, &sig, NULL);	/* ??? */
	sigaction(SIGSEGV, &sig, NULL);
	sigaction(SIGXCPU, &sig, NULL);
	sigaction(SIGXFSZ, &sig, NULL);
	sigaction(SIGSYS, &sig, NULL);

	/* 
	 * Set the signal handler for rereading local volume info.  
	 */
	sigfillset(&sig.sa_mask);
	sig.sa_handler = hup_sighandler;
	sig.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &sig, NULL);

	/* 
	 * Set the signal handler for terminating poll().  
	 */
	sigfillset(&sig.sa_mask);
	sig.sa_handler = dummy_sighandler;
	sig.sa_flags = 0;
	sigaction(SIGUSR1, &sig, NULL);

	/* 
	 * Ignore SIGPIPE.  
	 */
	sigemptyset(&sig.sa_mask);
	sig.sa_handler = SIG_IGN;
	sig.sa_flags = SA_RESTART;
	sigaction(SIGPIPE, &sig, NULL);
}

/* 
 *! Set default sighandlers.  
 */
static void disable_sig_handlers(void)
{
	struct sigaction sig;

	/* 
	 * Disable the sighandlers which were set.  
	 */
	sigfillset(&sig.sa_mask);
	sig.sa_handler = SIG_DFL;
	sig.sa_flags = 0;
	sigaction(SIGINT, &sig, NULL);
	sigaction(SIGQUIT, &sig, NULL);
	sigaction(SIGTERM, &sig, NULL);
	sigaction(SIGILL, &sig, NULL);
	sigaction(SIGBUS, &sig, NULL);
	sigaction(SIGFPE, &sig, NULL);
	sigaction(SIGTRAP, &sig, NULL);
	sigaction(SIGSEGV, &sig, NULL);
	sigaction(SIGXCPU, &sig, NULL);
	sigaction(SIGXFSZ, &sig, NULL);
	sigaction(SIGSYS, &sig, NULL);
	sigaction(SIGUSR1, &sig, NULL);
}

/* 
 *! Make zfsd to terminate.  
 */
void terminate(void)
{
	sigset_t mask, old_mask;

	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, &old_mask);
	exit_sighandler(0);
	pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
}

/* 
 *! Write a message and exit.  
 */
static void ATTRIBUTE_NORETURN die(void)
{
	message(LOG_EMERG, FACILITY_ZFSD, "ZFSD could not be started.\n");
	exit(EXIT_FAILURE);
}

/* 
 *! Initialize various data structures needed by ZFSD.  
 */
static bool initialize_data_structures(void)
{
	if (pthread_key_create(&thread_data_key, NULL))
		return false;
	if (pthread_key_create(&thread_name_key, NULL))
		return false;
	pthread_setspecific(thread_name_key, "Main thread");

	/* 
	 * Initialize data structures in other modules.  
	 */

	initialize_control_c();
	
	initialize_config_c();
	if (!initialize_random_c())
		return false;

	initialize_hardlink_list_c();
	initialize_metadata_c();
	initialize_journal_c();
	initialize_fh_c();
	initialize_file_c();
	initialize_cap_c();
	initialize_node_c();
	initialize_volume_c();
	initialize_zfs_prot_c();
	initialize_user_group_c();

	fd_data_init();

	return true;
}

/* 
 *! Destroy data structures.  
 */
static void cleanup_data_structures(void)
{
	fd_data_destroy();


	/* 
	 * Destroy data of config reader thread.  
	 */
	if (zfs_config.config_reader_data.thread_id)
	{
		pthread_join(zfs_config.config_reader_data.thread_id, NULL);
		zfs_config.config_reader_data.thread_id = 0;
		network_worker_cleanup(&zfs_config.config_reader_data);
		semaphore_destroy(&zfs_config.config_reader_data.sem);
	}

	/* 
	 * Destroy data structures in other modules.  
	 */
	cleanup_user_group_c();
	cleanup_zfs_prot_c();
	cleanup_volume_c();
	cleanup_node_c();
	cleanup_cap_c();
	cleanup_file_c();
	cleanup_fh_c();
	cleanup_journal_c();
	cleanup_metadata_c();
	cleanup_hardlink_list_c();
	cleanup_random_c();
	cleanup_config_c();

	// deinit dbus
	cleanup_control_c();

	pthread_key_delete(thread_data_key);
	pthread_key_delete(thread_name_key);
}

#ifdef DEBUG
static void log_arch_specific(void)
{
	message(LOG_DATA, FACILITY_DATA, "sizeof (pthread_mutex_t) = %u\n",
			sizeof(pthread_mutex_t));
	message(LOG_DATA, FACILITY_DATA, "sizeof (pthread_cond_t) = %u\n",
			sizeof(pthread_cond_t));
	message(LOG_DATA, FACILITY_DATA, "sizeof (thread) = %u\n", sizeof(thread));
	message(LOG_DATA, FACILITY_DATA, "sizeof (padded_thread) = %u\n",
			sizeof(padded_thread));
	message(LOG_DATA, FACILITY_DATA, "sizeof (internal_fh) = %u\n",
			sizeof(struct internal_fh_def));
	message(LOG_DATA, FACILITY_DATA, "sizeof (internal_dentry) = %u\n",
			sizeof(struct internal_dentry_def));
	message(LOG_DATA, FACILITY_DATA, "sizeof (internal_cap) = %u\n",
			sizeof(struct internal_cap_def));
	message(LOG_DATA, FACILITY_DATA, "sizeof (virtual_dir) = %u\n",
			sizeof(struct virtual_dir_def));
	message(LOG_DATA, FACILITY_DATA, "sizeof (fattr) = %u\n", sizeof(fattr));
	message(LOG_DATA, FACILITY_DATA, "sizeof (varray) = %u\n", sizeof(varray));
	message(LOG_DATA, FACILITY_DATA, "sizeof (metadata) = %u\n",
			sizeof(metadata));
	message(LOG_DATA, FACILITY_DATA, "sizeof (fh_mapping) = %u\n",
			sizeof(fh_mapping));

}
#endif

static void set_daemon_paging_strategy()
{
#ifdef HAVE_MLOCKALL
	/* 
	 * Keep the pages of the daemon in memory.  
	 */
	if (zfs_config.mlock_zfsd && mlockall(MCL_CURRENT | MCL_FUTURE))
	{
		message(LOG_CRIT, FACILITY_ZFSD, "mlockall: %s\n", strerror(errno));
		die();
	}
#endif
}

static void wait_for_pool_to_die(thread_pool * pool)
{
	wait_for_thread_to_die(&pool->main_thread, NULL);
	wait_for_thread_to_die(&pool->regulator_thread, NULL);
}

/*! \brief Keeps state of zlomekFS services */
typedef struct zfs_started_services_def
{
	/*! fuse thread is running */
	bool kernel_started;
	/*! server thread is running */
	bool network_started;
	/*! update thread is running */
	bool update_started;
#if defined ENABLE_HTTP_INTERFACE
	/*! http server is running */
	bool http_started;
#endif
}
zfs_started_services;

static int zfs_start_services(zfs_started_services * services)
{
	services->kernel_started = false;
	services->update_started = update_start();
	services->network_started = network_start (zfs_config.this_node.host_port);
	
	if (services->network_started != true || services->update_started != true)
	{
		terminate();
		return EXIT_FAILURE;
	}

#if defined ENABLE_HTTP_INTERFACE
	services->http_started = http_fs_start();
	if (services->http_started != true)
	{
		message(LOG_ERROR, FACILITY_ZFSD, "Failed to start http server\n");
		terminate();
		return EXIT_FAILURE;
	}
#endif

	bool rv = read_cluster_config();
	if (rv != true)
	{
		terminate();
		return EXIT_FAILURE;
	}

	update_node_name();

	if (!keep_running())
	{
		terminate();
		return EXIT_FAILURE;
	}

#ifdef ENABLE_FS_INTERFACE
	services->kernel_started = fs_start();
#endif

	zfsd_set_state(ZFSD_STATE_RUNNING);

	return EXIT_SUCCESS;
}


static void zfs_stop_services(zfs_started_services * services)
{
	if (services->update_started)
		wait_for_pool_to_die(&update_pool);

	if (services->network_started)
		wait_for_pool_to_die(&network_pool);

#ifdef ENABLE_FS_INTERFACE
#if defined HAVE_FUSE
	if (services->kernel_started)
	{
		wait_for_pool_to_die(&kernel_pool);
	}
#elif defined HAVE_DOKAN
#endif
#endif

	fd_data_shutdown();

	if (services->update_started)
		update_cleanup();

	if (services->network_started)
		network_cleanup();

#ifdef ENABLE_FS_INTERFACE
	if (services->kernel_started)
		fs_cleanup();
#endif

#if defined ENABLE_HTTP_INTERFACE
	if (services->http_started)
		http_fs_cleanup();
#endif
}

static void zfsd_main_loop(void)
{
	/* 
	 * Workaround valgrind bug (PR/77369), i.e. prevent from waiting
	 * for joinee threads while signal is received.  
	 */
#if 0
	printf("Keep running?\n");
	getc(stdin);
	pid_t pid = getpid();
	kill(pid, SIGTERM);
#else
	while (keep_running())
	{
		/* 
		 * Sleep gets interrupted by the signal.  
		 */
		pause();
	}
#endif
}

static int zfsd_main(void)
{

	set_daemon_paging_strategy();	

	zfs_started_services services;
	int rv = zfs_start_services(&services);
	zfsd_main_loop();
	zfs_stop_services(&services);

	return rv;
}

/* 
 *! Entry point of ZFS daemon.  
 */
int main(int argc, char **argv)
{
	zfs_openlog(argc, (const char **)argv);

	init_constants();
	init_sig_handlers();

	process_arguments(argc, argv);

	if (!initialize_data_structures())
	{
		die();
	}

	lock_info li[MAX_LOCKED_FILE_HANDLES];
	set_lock_info(li);

	int rv;
	rv = read_local_config_from_file(get_local_config_path());
	if (rv != CONFIG_TRUE)
	{
		//TODO: failed to read local config file
		die();
	}

#ifdef DEBUG
	log_arch_specific();
#endif

	int ret = zfsd_main();

	free_arguments();

	cleanup_data_structures();
	disable_sig_handlers();

	zfs_closelog();

	return ret;
}
