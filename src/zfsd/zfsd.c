/*! \file
    \brief ZFS daemon.  */

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

#include "system.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <ucontext.h>
#include "pthread.h"
#include "zfsd.h"
#include "memory.h"
#include "semaphore.h"
#include "log.h"
#include "hardlink-list.h"
#include "journal.h"
#include "config.h"
#include "thread.h"
#include "kernel.h"
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

#ifdef TEST
#include "test.h"
#endif

/*! Thread ID of the main thread.  */
pthread_t main_thread;

/*! Name of the configuration file.  */
static char *config_file;

#ifndef SI_FROMKERNEL
#define SI_FROMKERNEL(siptr)	((siptr)->si_code > 0)
#endif

/*! Signal handler for terminating zfsd.  */

static void
exit_sighandler (ATTRIBUTE_UNUSED int signum)
{
  message (LOG_NOTICE, NULL, "Entering exit_sighandler\n");

  zfsd_mutex_lock (&running_mutex);
  running = false;
  zfsd_mutex_unlock (&running_mutex);

  thread_pool_terminate (&kernel_pool);
  thread_pool_terminate (&network_pool);

  if (update_pool.regulator_thread)
    {
      queue_exiting (&update_queue);
      thread_pool_terminate (&update_pool);
    }

  thread_terminate_blocking_syscall (&cleanup_dentry_thread,
                                     &cleanup_dentry_thread_in_syscall);

  if (config_reader_data.thread_id)
    {
      set_thread_state (&config_reader_data, THREAD_DYING);
      semaphore_up (&config_sem, 1);
    }

  /* Terminate the sleep.  */
  pthread_kill (main_thread, SIGUSR1);

  message (LOG_NOTICE, NULL, "Leaving exit_sighandler\n");
}

/*! Report the fatal signal.  */

static void
fatal_sigaction (int signum, siginfo_t *info, void *data)
{
  /* Process only signals which are from kernel.  */
  if (SI_FROMKERNEL (info))
    {
      ucontext_t *context = (ucontext_t *) data;

      switch (signum)
        {
          case SIGBUS:
          case SIGSEGV:
#if defined (__linux__) && defined(__i386__)
            internal_error ("%s at %p when accessing %p", strsignal (signum),
                            context->uc_mcontext.gregs[REG_EIP],
                            info->si_addr);
#elif defined (__linux__) && defined(__x86_64__)
            internal_error ("%s at %p when accessing %p", strsignal (signum),
                            context->uc_mcontext.gregs[REG_RIP],
                            info->si_addr);
#else
            internal_error ("%s when accessing %p", strsignal (signum),
                            info->si_addr);
#endif
            break;

          case SIGILL:
          case SIGFPE:
            internal_error ("%s at %p", strsignal (signum), info->si_addr);
            break;

          default:
            internal_error ("%s", strsignal (signum));
            break;
        }
    }
}

/*! Signal handler for SIGHUP.
    \param signum Number of the received signal.  */

static void
hup_sighandler (ATTRIBUTE_UNUSED int signum)
{
  add_reread_config_request (&invalid_string, 0);
}

/*! Empty signal handler, used to break poll and other syscalls.  */

static void
dummy_sighandler (ATTRIBUTE_UNUSED int signum)
{
  message (LOG_INFO, NULL, "signalled %lu\n", pthread_self ());
}

/*! Initialize signal handlers.  */

static void
init_sig_handlers (void)
{
  struct sigaction sig;

  /* Remember the thread ID of this thread.  */
  main_thread = pthread_self ();

  /* Initialize the mutexes which are used with signal handlers.  */
  zfsd_mutex_init (&running_mutex);
  zfsd_mutex_init (&cleanup_dentry_thread_in_syscall);

  /* Set the signal handler for terminating zfsd.  */
  sigfillset (&sig.sa_mask);
  sig.sa_handler = exit_sighandler;
  sig.sa_flags = SA_RESTART;
  sigaction (SIGINT, &sig, NULL);
  sigaction (SIGQUIT, &sig, NULL);
  sigaction (SIGTERM, &sig, NULL);

  /* Set the signal handler for fatal errors.  */
  sigfillset (&sig.sa_mask);
  sig.sa_sigaction = fatal_sigaction;
  sig.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction (SIGILL, &sig, NULL);
  sigaction (SIGBUS, &sig, NULL);
  sigaction (SIGFPE, &sig, NULL);
  sigaction (SIGTRAP, &sig, NULL);	/* ??? */
  sigaction (SIGSEGV, &sig, NULL);
  sigaction (SIGXCPU, &sig, NULL);
  sigaction (SIGXFSZ, &sig, NULL);
  sigaction (SIGSYS, &sig, NULL);

  /* Set the signal handler for rereading local volume info.  */
  sigfillset (&sig.sa_mask);
  sig.sa_handler = hup_sighandler;
  sig.sa_flags = SA_RESTART;
  sigaction (SIGHUP, &sig, NULL);

  /* Set the signal handler for terminating poll().  */
  sigfillset (&sig.sa_mask);
  sig.sa_handler = dummy_sighandler;
  sig.sa_flags = 0;
  sigaction (SIGUSR1, &sig, NULL);

  /* Ignore SIGPIPE.  */
  sigemptyset (&sig.sa_mask);
  sig.sa_handler = SIG_IGN;
  sig.sa_flags = SA_RESTART;
  sigaction (SIGPIPE, &sig, NULL);
}

/*! Set default sighandlers.  */

static void
disable_sig_handlers (void)
{
  struct sigaction sig;

  /* Disable the sighandlers which were set.  */
  sigfillset (&sig.sa_mask);
  sig.sa_handler = SIG_DFL;
  sig.sa_flags = 0;
  sigaction (SIGINT, &sig, NULL);
  sigaction (SIGQUIT, &sig, NULL);
  sigaction (SIGTERM, &sig, NULL);
  sigaction (SIGILL, &sig, NULL);
  sigaction (SIGBUS, &sig, NULL);
  sigaction (SIGFPE, &sig, NULL);
  sigaction (SIGTRAP, &sig, NULL);
  sigaction (SIGSEGV, &sig, NULL);
  sigaction (SIGXCPU, &sig, NULL);
  sigaction (SIGXFSZ, &sig, NULL);
  sigaction (SIGSYS, &sig, NULL);
  sigaction (SIGUSR1, &sig, NULL);

  /* Destroy the mutexes which are used with signal handlers.  */
  zfsd_mutex_destroy (&cleanup_dentry_thread_in_syscall);
  zfsd_mutex_destroy (&running_mutex);
}

/*! Display the usage and arguments.  */
//TODO: replace chars and options with DEFINES (use them on all places
void
usage (void)
{
  printf ("Usage: zfsd [OPTION]...\n\n"
	  "  -o config=FILE               "
          "Specifies the name of the configuration file.\n"
	  "  -o node=ID:NAME:HOSTNAME     "
          "Fetch global configuration from specified node.\n"
	  "  -o loglevel=DEBUG_LEVEL                "
          "Display debugging messages up to level DEBUG_LEVEL.\n"
	  "                               "
	  "      --help                   "
          "Display this help and exit.\n"
	  "      --version                "
          "Output version information and exit.\n"
	  "\n"
	  "FUSE options:\n"
	  "  -d, -o debug                 "
	  "Enable debug output (implies -f)\n"
	  "  -f                           "
	  "Foreground operation\n");

	print_syplog_help (1, 1);
}

/*! Display the version, exit the program with exit code EXITCODE.  */

static void ATTRIBUTE_NORETURN
version (int exitcode)
{
  printf ("zfsd 0.1.0\n");
  printf ("Copyright (C) 2003, 2004 Josef Zlomek\n");
  printf ("This is free software; see the source for copying conditions.  There is NO\n");
  printf ("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

  exit (exitcode);
}

/*! For long options that have no equivalent short option, use a non-character
   as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  OPTION_HELP = CHAR_MAX + 1,
  OPTION_VERSION
};

static const struct fuse_opt main_options[] = {
  FUSE_OPT_KEY ("config=", 'f'),
  FUSE_OPT_KEY ("node=", 'n'),
  FUSE_OPT_KEY ("-v", 'v'),
  FUSE_OPT_KEY ("loglevel=", 'l'),
  FUSE_OPT_KEY ("--help", OPTION_HELP),
  FUSE_OPT_KEY ("--version", OPTION_VERSION),
  FUSE_OPT_END
};

/*! Process command line arguments.  */
static int handle_one_argument (ATTRIBUTE_UNUSED void *data,
				ATTRIBUTE_UNUSED const char *arg, int key,
				ATTRIBUTE_UNUSED struct fuse_args *outargs)
{
log_level_t verbose = DEFAULT_LOG_LEVEL;
  switch (key)
    {
    case 'f':
      free (config_file);
      config_file = xstrdup (strchr (arg, '=') + 1);
      return 0;

    case 'n':
      free (config_node);
      config_node = xstrdup (strchr (arg, '=') + 1);
      return 0;

    case 'l':
      verbose = atoi (strchr(arg,'=')+1);
      set_log_level (&syplogger, verbose);
      return 0;

    case OPTION_HELP:
      usage ();
      exit (EXIT_SUCCESS);
    case OPTION_VERSION:
      version (EXIT_SUCCESS);
      exit (EXIT_SUCCESS);

    case FUSE_OPT_KEY_OPT: case FUSE_OPT_KEY_NONOPT: default:
      return 1;
    }
}

static void
process_arguments (int argc, char **argv)
{
  main_args = (struct fuse_args) FUSE_ARGS_INIT (argc, argv);
  if (fuse_opt_parse (&main_args, NULL, main_options, handle_one_argument) != 0)
    {
      usage ();
      exit (EXIT_FAILURE);
    }
}
/*! Make zfsd to terminate.  */

void
terminate (void)
{
  sigset_t mask, old_mask;

  sigfillset (&mask);
  pthread_sigmask (SIG_SETMASK, &mask, &old_mask);
  exit_sighandler (0);
  pthread_sigmask (SIG_SETMASK, &old_mask, NULL);
}

/*! Write a message and exit.  */

static void ATTRIBUTE_NORETURN
die (void)
{
  message (LOG_EMERG, stderr, "ZFSD could not be started.\n");
  exit (EXIT_FAILURE);
}

/*! Initialize various data structures needed by ZFSD.  */

bool
initialize_data_structures (void)
{
  if (pthread_key_create (&thread_data_key, NULL))
    return false;
  if (pthread_key_create (&thread_name_key, NULL))
    return false;

  /* Initialize data structures in other modules.  */
  initialize_config_c ();
  if (!initialize_random_c ())
    return false;

  initialize_hardlink_list_c ();
  initialize_metadata_c ();
  initialize_journal_c ();
  initialize_fh_c ();
  initialize_file_c ();
  initialize_cap_c ();
  initialize_node_c ();
  initialize_volume_c ();
  initialize_zfs_prot_c ();
  initialize_user_group_c ();
  return true;
}

/*! Destroy data structures.  */

void
cleanup_data_structures (void)
{
  /* Destroy data of config reader thread.  */
  if (config_reader_data.thread_id)
    {
      pthread_join (config_reader_data.thread_id, NULL);
      config_reader_data.thread_id = 0;
      network_worker_cleanup (&config_reader_data);
      semaphore_destroy (&config_reader_data.sem);
    }

  /* Destroy data structures in other modules.  */
  cleanup_user_group_c ();
  cleanup_zfs_prot_c ();
  cleanup_volume_c ();
  cleanup_node_c ();
  cleanup_cap_c ();
  cleanup_file_c ();
  cleanup_fh_c ();
  cleanup_journal_c ();
  cleanup_metadata_c ();
  cleanup_hardlink_list_c ();
  cleanup_random_c ();
  cleanup_config_c ();

  pthread_key_delete (thread_data_key);
  pthread_key_delete (thread_name_key);
}

static void
daemon_mode (void)
{
}

/*! Entry point of ZFS daemon.  */

int
main (int argc, char **argv)
{
  lock_info li[MAX_LOCKED_FILE_HANDLES]; //NOTE: for what? macros for memory??? TODO: better name
  bool kernel_started = false;
  bool network_started = false;
  bool update_started = false;
  int ret = EXIT_SUCCESS;

  zfs_openlog();

  init_constants ();
  init_sig_handlers ();

#ifndef TEST
  config_file = xstrdup ("/etc/zfs/config");
#endif
  process_arguments (argc, argv);

  if (!initialize_data_structures ())
    {
#ifndef TEST
      free (config_file);
#endif
      die ();
    }
  set_lock_info (li);

#ifdef TEST
  fake_config ();
#else
  if (!read_config_file (config_file))
    {
      free (config_file);
      die ();
    }
  free (config_file);
#endif
  update_node_name ();


#ifdef DEBUG
  message (LOG_DATA, NULL, "sizeof (pthread_mutex_t) = %u\n", sizeof (pthread_mutex_t));
  message (LOG_DATA, NULL, "sizeof (pthread_cond_t) = %u\n", sizeof (pthread_cond_t));
  message (LOG_DATA, NULL, "sizeof (thread) = %u\n", sizeof (thread));
  message (LOG_DATA, NULL, "sizeof (padded_thread) = %u\n", sizeof (padded_thread));
  message (LOG_DATA, NULL, "sizeof (internal_fh) = %u\n", sizeof (struct internal_fh_def));
  message (LOG_DATA, NULL, "sizeof (internal_dentry) = %u\n", sizeof (struct internal_dentry_def));
  message (LOG_DATA, NULL, "sizeof (internal_cap) = %u\n", sizeof (struct internal_cap_def));
  message (LOG_DATA, NULL, "sizeof (virtual_dir) = %u\n", sizeof (struct virtual_dir_def));
  message (LOG_DATA, NULL, "sizeof (fattr) = %u\n", sizeof (fattr));
  message (LOG_DATA, NULL, "sizeof (varray) = %u\n", sizeof (varray));
  message (LOG_DATA, NULL, "sizeof (metadata) = %u\n", sizeof (metadata));
  message (LOG_DATA, NULL, "sizeof (fh_mapping) = %u\n", sizeof (fh_mapping));
#endif

  /* Keep the pages of the daemon in memory.  */
  if (mlock_zfsd && mlockall (MCL_CURRENT | MCL_FUTURE))
    {
      message (LOG_CRIT, stderr, "mlockall: %s\n", strerror (errno));
      die ();
    }

  daemon_mode ();

  fd_data_init ();

  /* Start the threads.  */
  update_started = update_start ();//NOTE:where checked?
  network_started = network_start ();
  kernel_started = false;

  if (network_started)
    {
#ifdef TEST
      test_zfs ();
#else
      if (!read_cluster_config ())
        {
          terminate ();
          ret = EXIT_FAILURE;
        }
#endif
    }
  update_node_name ();

  if (!network_started)
    {
      terminate ();
      ret = EXIT_FAILURE;
    }
  else if (running)
    {
      kernel_started = kernel_start ();
    }

  /* Workaround valgrind bug (PR/77369),
     i.e. prevent from waiting for joinee threads while signal is received.  */
  while (running)
    {
      /* Sleep gets interrupted by the signal.  */
      sleep (1000000);
    }

  if (update_started)
    {
      wait_for_thread_to_die (&update_pool.main_thread, NULL);
      wait_for_thread_to_die (&update_pool.regulator_thread, NULL);
    }
  if (network_started)
    {
      wait_for_thread_to_die (&network_pool.main_thread, NULL);
      wait_for_thread_to_die (&network_pool.regulator_thread, NULL);
    }
  if (kernel_started)
    {
      wait_for_thread_to_die (&kernel_pool.main_thread, NULL);
      wait_for_thread_to_die (&kernel_pool.regulator_thread, NULL);
    }

  fd_data_shutdown ();
#ifdef TEST
  test_cleanup ();
#endif

  if (update_started)
    update_cleanup ();
  if (network_started)
    network_cleanup ();
  if (kernel_started)
    kernel_cleanup ();

  fd_data_destroy ();

  cleanup_data_structures ();
  disable_sig_handlers ();

  return ret;
}
