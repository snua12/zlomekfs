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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#include "system.h"
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include "config.h"
#include "log.h"

/* Name of the configuration file.  */
static char *config_file = "/etc/zfs/config";

/* Initialize signal handlers.  */

static void
init_sig_handlers()
{
  struct sigaction sig;

  /* Set the signal handler for fatal errors.  */
  sigfillset(&sig.sa_mask);
  sig.sa_handler = fatal_sighandler;
  sig.sa_flags = 0;
  sigaction(SIGILL, &sig, NULL);
  sigaction(SIGBUS, &sig, NULL);
  sigaction(SIGFPE, &sig, NULL);
  sigaction(SIGTRAP, &sig, NULL);	/* ??? */
  sigaction(SIGSEGV, &sig, NULL);
  sigaction(SIGXCPU, &sig, NULL);
  sigaction(SIGXFSZ, &sig, NULL);
  sigaction(SIGSEGV, &sig, NULL);
  sigaction(SIGSYS, &sig, NULL);

  /* Ignore SIGPIPE.  */
  sigemptyset(&sig.sa_mask);
  sig.sa_handler = SIG_IGN;
  sig.sa_flags = SA_RESTART;
  sigaction(SIGPIPE, &sig, NULL);
}

/* Display the usage and arguments, exit the program with exit code EXITCODE.  */

static void
usage(int exitcode)
{
  printf("Usage: zfsd [OPTION]...\n\n");
  printf("  -v, --verbose  Verbose; display verbose debugging messages.\n");
  printf("                 Multiple -v increases verbosity.\n");
  printf("  -q, --quiet    Quiet; display less messages.\n");
  printf("                 Multiple -q increases quietness.\n");
  printf("      --help     display this help and exit\n");
  printf("      --version  output version information and exit\n");

  exit(exitcode);
}

/* Display the version, exit the program with exit code EXITCODE.  */

static void
version(int exitcode)
{
  printf("zfsd 0.1.0\n");
  printf("Copyright (C) 2003 Josef Zlomek\n");
  printf("This is free software; see the source for copying conditions.  There is NO\n");
  printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

  exit(exitcode);
}

/* For long options that have no equivalent short option, use a non-character
   as a pseudo short option, starting with CHAR_MAX + 1.  */
enum long_option
{
  OPTION_HELP = CHAR_MAX + 1,
  OPTION_VERSION
};

static struct option const long_options[] =
{
    {"verbose", no_argument, 0, 'v'},
    {"quiet", no_argument, 0, 'q'},
    {"help", no_argument, 0, OPTION_HELP},
    {"version", no_argument, 0, OPTION_VERSION},
    {NULL, 0, NULL, 0}
};

/* Process command line arguments.  */

static void
process_arguments(int argc, char **argv)
{
  int c;

  while ((c = getopt_long(argc, argv, "qv", long_options, NULL)) != -1)
    {
      switch (c)
	{
	  case 'v':
	    verbose++;
	    break;

	  case 'q':
	    verbose--;
	    break;

	  case OPTION_HELP:
	    usage(EXIT_SUCCESS);
	    break;

	  case OPTION_VERSION:
	    version(EXIT_SUCCESS);
	    break;

	  default:
	    usage(EXIT_FAILURE);
	    break;
	}
    }
}

/* Entry point of ZFS daemon.  */

int
main(int argc, char **argv)
{
  init_sig_handlers();

  process_arguments(argc, argv);

  if (!read_config(config_file))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
