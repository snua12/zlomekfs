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
#include <signal.h>
#include "config.h"

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

/* Entry point of ZFS daemon.  */

int
main(int argc, char **argv)
{
  init_sig_handlers();

  if (!read_config(config_file))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
