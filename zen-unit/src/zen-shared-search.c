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

#define _GNU_SOURCE
#include <link.h>
#include <string.h>


#include "zen-elf-search.h"
#include "zen-shared-search.h"


typedef struct callback_info_def
{
	report_callback_def callback_func;
	void * callback_data;
} * callback_info;

int process_shared_library (struct dl_phdr_info * info, size_t size, void * callback)
{

	if (info->dlpi_name != NULL && strlen (info->dlpi_name) != 0)
		return walk_elf_file (info->dlpi_name,
				((callback_info)callback)->callback_func,
				((callback_info)callback)->callback_data,
				info->dlpi_addr);
}


zen_error walk_loaded_libraries ( report_callback_def callback_func, void * data)
{
	struct callback_info_def callback = {callback_func, data};

	dl_iterate_phdr( process_shared_library, &callback);

	return ZEN_NOERR;
}
