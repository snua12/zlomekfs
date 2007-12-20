#ifndef		ZEN_SHARED_SEARCH_H
#define		ZEN_SHARED_SEARCH_H

/*! \file
    \brief 
*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Zen Unit.

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

#include "zen-elf-search.h"
#include "zen-error.h"

#ifdef __cplusplus
extern "C"
{
#endif

zen_error walk_loaded_libraries ( report_callback_def callback_func, void * data);

#ifdef __cplusplus
}
#endif

#endif		/* ZEN_SHARED_SEARCH_H */
