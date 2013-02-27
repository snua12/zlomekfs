/* ! \file \brief Shared memory reader implementation.

   Shm medium handles low level access to shared memory for readers and
   writers. TODO: describe behaviour of fixed sizes (of max size) */

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */


#define _GNU_SOURCE

#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <android/log.h>

#undef _GNU_SOURCE

#include "logcat-medium.h"


void print_logcat_medium_help( __attribute__ ((__unused__)) int fd,  __attribute__ ((__unused__)) int tabs)
{

}

  // table with known param types
static const struct option option_table[] = {
	{NULL, 0, NULL, 0}
};

bool_t is_logcat_medium_arg(const char *arg)
{
	return opt_table_contains((struct option *)option_table, arg);
}

// / Initializes logcat specific parts of reader structure
syp_error open_logcat_medium(__attribute__ ((__unused__)) medium target,
		__attribute__ ((__unused__)) int argc, __attribute__ ((__unused__)) const char **argv)
{

	target->open_medium = open_logcat_medium;
	target->close_medium = close_logcat_medium;
	target->access_medium = logcat_access;
	return NOERR;
}

// / Close and destroys logcat reader specific parts of reader strucutre
syp_error close_logcat_medium(__attribute__ ((__unused__)) medium target)
{
	return NOERR;
}

#define PTRid PRIu64
#define PTRid_conversion (uint64_t)

syp_error logcat_access(__attribute__ ((__unused__)) medium target, log_struct message)
{
#ifdef ENABLE_CHECKING
	if (target == NULL || log == NULL)
		return ERR_BAD_PARAMS;
	if (target->kind == NO_OPERATION || target->used_formatter == NULL ||
		target->type_specific == NULL || log == NULL)
		return ERR_NOT_INITIALIZED;

#endif
	// check boundaries
	if (target->length > 0 &&
		target->length - target->pos <
		target->used_formatter->get_max_print_size())
	{
		// move to front
		target->pos = 0;
	}

	int32_t chars_accessed = 0;

	switch (target->kind)
	{
	case READ_LOG:
		break;
	case WRITE_LOG:
		__android_log_print(ANDROID_LOG_INFO, "zlomekFS", "%s\t%s\t%"PTRid"/%s\t%s\t%s\t%s",
			message->hostname, message->node_name, PTRid_conversion message->thread_id,
			message->thread_name, facility_to_name(message->facility),
			log_level_to_name(message->level),
			message->message);
		break;
	default:
		break;
	}

	return NOERR;
}
