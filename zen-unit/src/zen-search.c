/*! \file
    \brief Elf search implementation.

  Zen-unit is minimalistic approach to unit testing.

*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Zen Unit.

   Zen-unit is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Zen-unit is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Zen-unit; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/


#include <sys/types.h>
#include <regex.h>
#include <pthread.h>
#include <stdio.h>

#include "zen-static-search.h"
#include "zen-shared-search.h"
#include "zen-elf-search.h"

#include "zen-search.h"
#include "zen-defs.h"
#include "zen-unit.h"

regex_t compiled_match;
bool_t match_initialized = FALSE;

bool_t compile_regex()
{
	int ext_return = 0;
	
	ext_return = regcomp (&compiled_match, ZEN_NAME_REGEX, REG_EXTENDED | REG_NOSUB);
	if (ext_return != 0)
	{
		char err_buffer [1024];

		regerror (ext_return, &compiled_match, err_buffer, 1024);
		report_error ("fail to compile regex due to (%d) %s\n", ext_return, err_buffer);

		return FALSE;
	}

	return TRUE;
}

bool_t name_match (const char * name)
{
	if (match_initialized == FALSE)
		zen_search_init ();

	if (regexec (&compiled_match, name, 0, NULL, 0) == 0)
		return TRUE;

	return FALSE;
}

void zen_search_init (void)
{
	if ((match_initialized = compile_regex()) == FALSE)
		fail(ZEN_ERR_INTERNAL, "can't compile match regexp\n");
}

void zen_search_destroy (void)
{
	if (match_initialized)
		regfree (&compiled_match);
}

typedef struct callback_holder_def
{
	zen_test tests;
	int max_tests;
	int test_count;
	pthread_mutex_t mutex;
} * callback_holder;

void report_symbol (const char * name, void * func_ptr, callback_holder data)
{
	int pos = 0;
	pthread_mutex_lock (&(data->mutex));

	if (data->test_count >= data->max_tests)
		goto FINISHING;

	if (name_match (name) == TRUE)
	{
//		printf ("%d:catch function %s at %p\n", data->test_count, name, func_ptr);
		for (pos = 0; pos < data->test_count - 1; pos ++)
			if (data->tests[pos].function_ptr == func_ptr)
				break;

		if (data->tests[pos].function_ptr != func_ptr)
		{
			data->tests[data->test_count].function_ptr = func_ptr;
			snprintf (data->tests[data->test_count].name, NAME_LEN, "%s", name);
			data->test_count ++;
		}
	}

FINISHING:
	pthread_mutex_unlock (&(data->mutex));
}

zen_error get_test_functions (zen_test target, int * size)
{
	zen_error ret_code = ZEN_NOERR;
	struct callback_holder_def holder = 
	{
		.tests = target,
		.max_tests = *size,
		.test_count = 0,
		.mutex = PTHREAD_MUTEX_INITIALIZER
	};

	*size = 0;

	ret_code = walk_self_exe ((report_callback_def)report_symbol, &holder);
	if (ret_code != ZEN_NOERR)
		goto FINISHING;

	ret_code = walk_loaded_libraries ((report_callback_def)report_symbol, &holder);
	if (ret_code != ZEN_NOERR)
		goto FINISHING;

	*size = holder.test_count;

FINISHING:
	pthread_mutex_destroy (&(holder.mutex));

	return ret_code;
}
