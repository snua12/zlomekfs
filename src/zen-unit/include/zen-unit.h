#ifndef		ZEN_UNIT_H
#define		ZEN_UNIT_H

/*! \file
    \brief Interface for zen-unit

  Zen-unit is minimalistic approach to unit testing.

  Interface is as simple as two macros and there is no need to list tests somewhere.

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

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef FALSE
#define	FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

#define ZEN_ASSERT(test,message) do { if (!(test)) { printf("%s\n", message); return FALSE; } } while (0)

#define C_IDENTIFIER_CHARS	"-_a-zA-Z0-9"

#define ZEN_NAME_REGEX		"zen_"  "[" C_IDENTIFIER_CHARS "]+"  "_test"

#define ZEN_TEST(name)		int zen_ ## name ## _test (void * param)

typedef int (*zen_test_template) (void *);

#ifdef __cplusplus
}
#endif


#endif		/* ZEN_UNIT_H */
