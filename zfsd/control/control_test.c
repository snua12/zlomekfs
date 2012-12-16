/**
 *  \file control_test.c
 * 
 *  \author Ales Snuparek
 *
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

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

#include "system.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "control.h"
#include "zfsd_state.h"
#include "syplog.h"
#include "syplog_wrapper.h"

syp_error control_wrap_set_log_level(ATTRIBUTE_UNUSED logger glogger,
									 ATTRIBUTE_UNUSED log_level_t level)
{
	return 0;
}

syp_error control_wrap_set_facility(ATTRIBUTE_UNUSED logger glogger,
									ATTRIBUTE_UNUSED facility_t facility)
{
	return 0;
}

syp_error control_wrap_reset_facility(ATTRIBUTE_UNUSED logger glogger,
									  ATTRIBUTE_UNUSED facility_t facility)
{
	return 0;
}

/* dummy zfsd_get_state implementation */
zfsd_state_e zfsd_get_state(void)
{
	return ZFSD_STATE_STARTING;
}

static bool run = true;

static void sighandler(ATTRIBUTE_UNUSED int signum)
{
	run = false;
}

static void init_sighandler(void)
{
	struct sigaction sig;
	sig.sa_handler = sighandler;
	sig.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &sig, NULL);
	sigaction(SIGINT, &sig, NULL);
}

int main(ATTRIBUTE_UNUSED int argc, ATTRIBUTE_UNUSED char *argv[])
{
	initialize_control_c();
	init_sighandler();
	printf("Initialized\n");
	while (run == true)
	{
		sleep(1);
	}
	cleanup_control_c();
	printf("Stopped\n");
	return 0;
}
