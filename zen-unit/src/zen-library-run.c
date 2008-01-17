/*! \file
    \brief

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

#include "zen-defs.h"
#include "zen-search.h"

struct zen_test_def tests [MAX_TESTS];
int test_count = MAX_TESTS;


void LIB_CONSTRUCTOR init (void)
{
	zen_error ret_code = ZEN_NOERR;
	int index = 0;
	int ret = 0;

	zen_search_init ();

	ret_code = get_test_functions (tests, &test_count);
	if (ret_code != ZEN_NOERR)
		goto FINISHING;

	printf ("got %d functions\n", test_count);

	for (index = 0; index < test_count; index ++)
	{
		printf ("calling function %s\n", tests[index].name);
		ret = tests[index].function_ptr (NULL);
		if (ret != 0)
		{
			printf ("function failed with errcode %d\n", ret);
		}
	}

FINISHING:
	zen_search_destroy ();
	exit (ret_code);
}
