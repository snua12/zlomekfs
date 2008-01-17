#ifndef		ZEN_SEARCH_H
#define		ZEN_SEARCH_H

/*! \file
    \brief Elf search interface.

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

#include "zen-error.h"
#include "zen-unit.h"
#include "zen-defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct zen_test_def
{
	zen_test_template function_ptr;
	char name [NAME_LEN];
}* zen_test;

void zen_search_init (void);
void zen_search_destroy (void);

zen_error get_test_functions (zen_test target, int * size);


#ifdef __cplusplus
}
#endif


#endif		/* ZEN_SEARCH_H */
