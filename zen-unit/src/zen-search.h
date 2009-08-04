#ifndef		ZEN_SEARCH_H
#define		ZEN_SEARCH_H

/*! \file
    \brief Elf search interface.

  This file defines api for global search within process memory map for
  all tests in main binary and all libraries loaded.

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

#include "zen-error.h"
#include "zen-unit.h"
#include "zen-defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// structure holding information about one zen test.
typedef struct zen_test_def
{
	/// pointer to the test function (callable)
	zen_test_template function_ptr;
	/// test name (currently function / symbol name)
	char name [NAME_LEN];
	/// result of test call
	int result;
}* zen_test;

/** Initialize search structures. */
void zen_search_init (void);

/** Destroy search structures and free memory. */
void zen_search_destroy (void);

/** Search for tests and fill information about them
  into structure. Don't execute them.
 *
 * @param target array of uninitialized zen_test structures to fill
   with tests. Size of *size, here should be tests found inserted.
 * @param size upon call it contains length of target array,
   upon return it should be set to actual count of valid items in
   target array.
 * @return std errors
*/
zen_error get_test_functions (zen_test target, int * size);


#ifdef __cplusplus
}
#endif


#endif		/* ZEN_SEARCH_H */
