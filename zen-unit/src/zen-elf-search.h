#ifndef		ZEN_ELF_SEARCH
#define		ZEN_ELF_SEARCH

/*! \file
    \brief Header for functions which are capable of searching for symbols in elf files.

  Caller will provide function callback for reporting symbols, pointer to data structure 
  where callback function should store data and offset where file is mapped into memory.
  
  Then walk_elf_file will walk through elf file and calls
  callback_function (symbol_name, symbol_address, data).

  Any information usefull for walk_elf_file caller should be stored in data structure by callback.
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

#include "zen-error.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Callback function typedef. Will be called for every function symbol found.
 *
 * @param name name of symbol found
 * @param func_ptr value of symbol (we are interested only in functions)
 * @param data auxiliary function data provided with handler 
  (all calls will get the same). In this function should callbacks store
  any data needed and report for walk_* caller should be stored here too.
*/
typedef void (*report_callback_def) (const char * name, void * func_ptr, void * data);


/** Search for function symbols in elf file
 *
 * @param name name of elf file (relative or absolute path)
 * @param callback_func function pointer of callback that will be called for every symbol found.
 * @param data statefull data holder (sometimes called cookie) for functions. Report information should
   be stored here too. Will be given to every call of callback
 * @param offset offset on which elf file is loaded in memory
 * @return std errors
 * @see report_callback_def
 * @see walk_sections
*/
zen_error walk_elf_file (const char * name, report_callback_def callback_func, 
	void * data, off_t offset);

#ifdef __cplusplus
}
#endif

#endif		/* ZEN_ELF_SEARCH */
