/* ZFS daemon.
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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <ucontext.h>
#include "pthread.h"
#include "semaphore.h"
#include "memory.h"
#include "config.h"
#include "fh.h"
#include "cap.h"
#include "dir.h"
#include "file.h"
#include "log.h"
#include "node.h"
#include "volume.h"
#include "thread.h"
#include "client.h"
#include "network.h"
#include "zfsd.h"
#include "constant.h"
#include "random.h"

#ifdef TEST
#include "dir.h"
#endif

/* Name of the configuration file.  */
char *config_file = "/etc/zfs/config";

/* Data for main thread.  */
thread main_thread_data;

/* Local function prototypes.  */
static void terminate ();
static void exit_sighandler (int signum);
static void fatal_sigaction (int signum, siginfo_t *info, void *data);
static void init_sig_handlers ();
static void usage (int exitcode) ATTRIBUTE_NORETURN;
static void version (int exitcode) ATTRIBUTE_NORETURN;
static void process_arguments (int argc, char **argv);
static void die () ATTRIBUTE_NORETURN;

#ifndef SI_FROMKERNEL
#define SI_FROMKERNEL(siptr)	((siptr)->si_code > 0)
#endif

/* Make zfsd to terminate.  */

static void
terminate ()
{
  sigset_t mask, old_mask;

  sigfillset (&mask);
  pthread_sigmask (SIG_SETMASK, &mask, &old_mask);
  exit_sighandler (0);
  pthread_sigmask (SIG_SETMASK, &old_mask, NULL);
}

/* Signal handler for terminating zfsd.  */

static void
exit_sighandler (int signum)
{
  set_running (false);
  thread_terminate_poll (main_client_thread, &main_client_thread_in_poll);
  thread_terminate_poll (main_network_thread, &main_network_thread_in_poll);
}

/* Report the fatal signal.  */

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
#if defined(__i386__)
	    internal_error ("%s at %p when accessing %p", strsignal (signum),
			    context->uc_mcontext.gregs[REG_EIP],
			    info->si_addr);
#elif defined(__x86_64__)
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

/* Empty signal handler, used to break poll.  */

static void
dummy_sighandler (int signum)
{
}

/* Initialize signal handlers.  */

static void
init_sig_handlers ()
{
  struct sigaction sig;

  /* Remember the thread ID of this thread.  */
  main_thread = pthread_self ();

  /* Initialize the mutexes which are used with signal handlers.  */
  zfsd_mutex_init (&running_mutex);
  zfsd_mutex_init (&main_client_thread_in_poll);
  zfsd_mutex_init (&main_network_thread_in_poll);

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

/* Display the usage and arguments, exit the program with exit code EXITCODE.  */

static void
usage (int exitcode)
{
  printf ("Usage: zfsd [OPTION]...\n\n");
  printf ("  -f, --config=FILE  Specifies the name of the configuration file.\n");
  printf ("  -v, --verbose      Verbose; display verbose debugging messages.\n");
  printf ("                     Multiple -v increases verbosity.\n");
  printf ("  -q, --quiet        Quiet; display less messages.\n");
  printf ("                     Multiple -q increases quietness.\n");
  printf ("      --help         Display this help and exit.\n");
  printf ("      --version      Output version information and exit.\n");

  exit (exitcode);
}

/* Display the version, exit the program with exit code EXITCODE.  */

static void
version (int exitcode)
{
  printf ("zfsd 0.1.0\n");
  printf ("Copyright (C) 2003 Josef Zlomek\n");
  printf ("This is free software; see the source for copying conditions.  There is NO\n");
  printf ("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

  exit (exitcode);
}

/* For long options that have no equivalent short option, use a non-character
   as a pseudo short option, starting with CHAR_MAX + 1.  */
enum long_option
{
  OPTION_HELP = CHAR_MAX + 1,
  OPTION_VERSION
};

static struct option const long_options[] = {
  {"config", required_argument, 0, 'f'},
  {"verbose", no_argument, 0, 'v'},
  {"quiet", no_argument, 0, 'q'},
  {"help", no_argument, 0, OPTION_HELP},
  {"version", no_argument, 0, OPTION_VERSION},
  {NULL, 0, NULL, 0}
};

/* Process command line arguments.  */

static void
process_arguments (int argc, char **argv)
{
  int c;
  int config_file_allocated = 0;

  while ((c = getopt_long (argc, argv, "f:qv", long_options, NULL)) != -1)
    {
      switch (c)
	{
	  case 'f':
	    if (config_file_allocated)
	      free (config_file);
	    config_file = xstrdup (optarg);
	    config_file_allocated = 1;
	    break;

	  case 'v':
	    verbose++;
	    break;

	  case 'q':
	    verbose--;
	    break;

	  case OPTION_HELP:
	    usage (EXIT_SUCCESS);
	    break;

	  case OPTION_VERSION:
	    version (EXIT_SUCCESS);
	    break;

	  default:
	    usage (EXIT_FAILURE);
	    break;
	}
    }

  if (optind < argc)
    usage (EXIT_FAILURE);
}

#if 0
#include "splay-tree.h"
static void
test_splay ()
{
  splay_tree st;
  int i;

  st = splay_tree_create (200, NULL);
  for (i = 0; i <= 4; i++)
    splay_tree_insert (st, 2 * i, i);
  splay_tree_lookup (st, 7);
  debug_splay_tree (st);
}

#include "interval.h"
static void
test_interval ()
{
  interval_tree t;

  t = interval_tree_create (6);
  interval_tree_insert (t, 0, 4);
  interval_tree_insert (t, 10, 15);
  interval_tree_insert (t, 20, 25);
  interval_tree_insert (t, 30, 32);
  interval_tree_insert (t, 40, 45);
  interval_tree_insert (t, 50, 55);
  interval_tree_insert (t, 60, 65);
  debug_interval_tree (t);
}
#endif

/* Write a message and exit.  */

static void
die ()
{
  message (-2, stderr, "ZFSD could not be started.\n");
  exit (EXIT_FAILURE);
}

/* Initialize various data structures needed by ZFSD.  */

bool
initialize_data_structures ()
{
  if (pthread_key_create (&thread_data_key, NULL))
    return false;

  /* Initialize main thread data.  */
  semaphore_init (&main_thread_data.sem, 0);
  network_worker_init (&main_thread_data);
  pthread_setspecific (thread_data_key, &main_thread_data);

  /* Initialize data structures in other modules.  */
  if (!initialize_random_c ())
    return false;

  initialize_fh_c ();
  initialize_cap_c ();
  initialize_node_c ();
  initialize_volume_c ();
  initialize_zfs_prot_c ();
  return true;
}

/* Destroy data structures.  */

void
cleanup_data_structures ()
{
  /* Destroy main thread data.  */
  network_worker_cleanup (&main_thread_data);
  semaphore_destroy (&main_thread_data.sem);

  /* Destroy data structures in other modules.  */
  cleanup_zfs_prot_c ();
  cleanup_volume_c ();
  cleanup_node_c ();
  cleanup_cap_c ();
  cleanup_fh_c ();
  cleanup_random_c ();

  free (node_name);
}

#ifdef TEST

/* Testing configuration until configuration reading is programmed.  */

void
fake_config ()
{
  node nod;
  volume vol;

  get_node_name ();

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (1, "orion");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (1);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir1", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume1", "/volume1", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (2);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume2", "/volume2", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (2, "sabbath");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (3);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir1", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume3", "/volume1/volume3", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (4);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume4", "/volume2/sabbath/volume4", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  zfsd_mutex_lock (&node_mutex);
  nod = node_create (3, "jaro");
  zfsd_mutex_unlock (&node_mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (5);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "/home/joe/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume5", "/jaro/volume5", nod);
  zfsd_mutex_unlock (&vol->mutex);

  zfsd_mutex_lock (&volume_mutex);
  vol = volume_create (6);
  zfsd_mutex_unlock (&volume_mutex);
  if (nod == this_node)
    volume_set_local_info (vol, "home/joe/.zfs/dir2", VOLUME_NO_LIMIT);
  volume_set_common_info (vol, "volume6", "/volume6", nod);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&nod->mutex);

  debug_virtual_tree ();
}

/* Test functions accessing ZFS.  */

void
test_zfs (thread *t)
{
  dir_op_res res;
  int test = 0;
  string rmdir_name = {3, "dir"};

  if (strcmp (node_name, "orion") == 0)
    {
      node nod;
      char *str;

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST %d\n", ++test);
      zfs_proc_null_client (t, NULL, nod);

      zfsd_mutex_lock (&node_mutex);
      nod = node_lookup (2);
      zfsd_mutex_unlock (&node_mutex);
      message (2, stderr, "TEST %d\n", ++test);
      zfs_proc_root_client (t, NULL, nod);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/volume3/subdir/file");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/volume3/subdir");
      printf ("%d\n", zfs_extended_lookup (&res, &root_fh, str));
      free (str);

      message (2, stderr, "TEST %d\n", ++test);
      str = xstrdup ("/volume1/volume3/subdir/dir");
      printf ("%d\n", zfs_rmdir (&res.file, &rmdir_name));
      free (str);
    }
}
#endif

static void
daemon_mode ()
{
}

/* Entry point of ZFS daemon.  */

int
main (int argc, char **argv)
{
  init_constants ();
  init_sig_handlers ();

  process_arguments (argc, argv);

#if 0
  test_interval ();
  test_splay ();
#endif

  if (!initialize_data_structures ())
    die ();

#ifdef TEST
  fake_config ();
#else
  if (!read_config_file (config_file))
    die ();
#endif

  printf ("sizeof (thread) = %d\n", sizeof (thread));
  printf ("sizeof (padded_thread) = %d\n", sizeof (padded_thread));

#if 0
  /* Temporarily disable because it needs root privileges.  */
  /* Keep the pages of the daemon in memory.  */
  if (mlockall (MCL_CURRENT | MCL_FUTURE))
    {
      message (-1, stderr, "mlockall: %s\n", strerror (errno));
      die ();
    }
#endif

  daemon_mode ();

  /* Initialize information about network file descriptors.  */
  if (!init_network_fd_data ())
    die ();

  /* Create client threads and related threads.  */
  create_client_threads ();

  /* Create network threads and related threads.  */
  create_network_threads ();

  /* Make the connection with kernel and start main client thread.  */
  if (!client_start ())
    terminate ();

  /* Register the ZFS protocol RPC server, register_server never returns (unless
     error occurs).  */
#ifdef RPC
  register_server ();
#else
  if (network_start ())
    {
#ifdef TEST
      test_zfs (&main_thread_data);
#else
      if (!read_cluster_config ())
	terminate ();
#endif

      pthread_join (main_network_thread, NULL);
    }
  else
    terminate ();
#endif

  client_cleanup ();
  network_cleanup ();

  cleanup_data_structures ();

  return EXIT_FAILURE;
}
