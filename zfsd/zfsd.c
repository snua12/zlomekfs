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
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include "alloc-pool.h"
#include "hashtab.h"
#include "config.h"
#include "fh.h"
#include "log.h"
#include "memory.h"
#include "node.h"
#include "volume.h"
#include "client.h"
#include "server.h"
#include "zfsd.h"

/* Name of the configuration file.  */
char *config_file = "/etc/zfs/config";

/* Local function prototypes.  */
static void init_sig_handlers ();
static void usage (int exitcode) ATTRIBUTE_NORETURN;
static void version (int exitcode) ATTRIBUTE_NORETURN;
static void process_arguments (int argc, char **argv);
static void die () ATTRIBUTE_NORETURN;

/* Initialize signal handlers.  */

static void
init_sig_handlers ()
{
  struct sigaction sig;

  /* Set the signal handler for fatal errors.  */
  sigfillset (&sig.sa_mask);
  sig.sa_handler = fatal_sighandler;
  sig.sa_flags = 0;
  sigaction (SIGILL, &sig, NULL);
  sigaction (SIGBUS, &sig, NULL);
  sigaction (SIGFPE, &sig, NULL);
  sigaction (SIGTRAP, &sig, NULL);	/* ??? */
  sigaction (SIGSEGV, &sig, NULL);
  sigaction (SIGXCPU, &sig, NULL);
  sigaction (SIGXFSZ, &sig, NULL);
  sigaction (SIGSEGV, &sig, NULL);
  sigaction (SIGSYS, &sig, NULL);

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

/* Write a message and exit.  */

static void
die ()
{
  message (-2, stderr, "ZFSD could not be started.\n");
  exit (EXIT_FAILURE);
}

/* Initialize various data structures needed by ZFSD.  */

void
initialize_data_structures ()
{
  initialize_fh_c ();
  initialize_node_c ();
  initialize_volume_c ();
}

/* Destroy data structures.  */

void
cleanup_data_structures ()
{
  cleanup_volume_c ();
  cleanup_node_c ();
  cleanup_fh_c ();
}

/* Entry point of ZFS daemon.  */

int
main (int argc, char **argv)
{
  init_sig_handlers ();

  process_arguments (argc, argv);

#if 0
  test_interval ();
  test_splay ();
#endif
  
  initialize_data_structures ();
  
  if (!read_config (config_file))
    die ();

#if 0
  /* Temporarily disable because it needs root privileges.  */
  /* Keep the pages of the daemon in memory.  */
  if (mlockall (MCL_CURRENT | MCL_FUTURE))
    {
      message (-1, stderr, "mlockall: %s\n", strerror (errno));
      die ();
    }
#endif

  /* Make the connection with kernel.  */
  if (!initialize_client ())
    die ();

  /* Create client threads and related threads.  */
  create_client_threads ();

  /* Create server threads and related threads.  */
  create_server_threads ();

  /* Register the ZFS protocol RPC server, register_server never returns (unless
     error occurs).  */
  register_server ();

  /* FIXME: kill threads.  */

  cleanup_data_structures ();

  return EXIT_FAILURE;
}
