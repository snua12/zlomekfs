#ifndef		ZEN_DEFS_H
#define		ZEN_DEFS_H

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


#include "zen-unit.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef int	bool_t;

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

#define		MAX_TESTS	1024

#define		NAME_LEN	64

#define LIB_CONSTRUCTOR __attribute__((constructor))
#define LIB_DESTRUCTOR __attribute__((destructor))


#ifdef __cplusplus
}
#endif

#endif		/* ZEN_DEFS_H */
