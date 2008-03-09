#ifndef		ZEN_SHARED_SEARCH_H
#define		ZEN_SHARED_SEARCH_H

/*! \file
    \brief Definitions of functions searching in shared objects for zen-unit test functions.
    @see zen-elf-search.h

  Functions here defined are wrappers about zen-elf-search functins,
  they only search for shared libraries and calls zen-elf-search's functions
  for every shared object found.
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

#include "zen-elf-search.h"
#include "zen-error.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Walk through linker structures looking for shared objects that we have
  and call walk_elf_file on all of them.
 *
 * @param callback_func callback function for reporting function symbols
 * @param data cookie structure for callback_func
 * @return std errors
 * @see report_callback_def
 * @see walk_elf_file
*/
zen_error walk_loaded_libraries ( report_callback_def callback_func, void * data);

#ifdef __cplusplus
}
#endif

#endif		/* ZEN_SHARED_SEARCH_H */
