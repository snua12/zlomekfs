/* Environment definitions.
   Copyright (C) 2003, 2004 Josef Zlomek

   This file is part of ZFS.

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
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef SYSTEM_H
#define SYSTEM_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* ZFSD is multi-threaded so it needs reentrant (ie. thread safe) functions */
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _THREAD_SAFE
#define _THREAD_SAFE
#endif

/* bool type and constants.  */
#ifndef bool
#define bool char
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* We want print format specifiers from <inttypes.h>  */
#ifdef __cplusplus
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

/* Offset of field in structure relative to structure's beginning.  */
#ifndef offsetof
#define offsetof(TYPE, MEMBER)  ((unsigned long) &((TYPE *) 0)->MEMBER)
#endif

/* Crash when executing this macro.  */
#define CRASH (*(char *) 0 = 0)

/* Boolean value whether checking is enabled.  */
#ifdef ENABLE_CHECKING
#define ENABLE_CHECKING_VALUE true
#else
#define ENABLE_CHECKING_VALUE false
#endif

/* Define valgrind macros.  */
#ifdef ENABLE_VALGRIND_CHECKING
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_NOACCESS(x,y)
#define VALGRIND_MAKE_WRITABLE(x,y)
#define VALGRIND_MAKE_READABLE(x,y)
#define VALGRIND_DISCARD(x)
#endif

/* Definitions of some GCC attributes.  */
#ifdef __GNUC__

#ifndef GCC_VERSION
#define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#endif /* GCC_VERSION */

/* Attribute __malloc__ on functions was valid as of gcc 2.96. */
#ifndef ATTRIBUTE_MALLOC
# if (GCC_VERSION >= 2096)
#  define ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define ATTRIBUTE_MALLOC
# endif /* GNUC >= 2.96 */
#endif /* ATTRIBUTE_MALLOC */

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif /* ATTRIBUTE_UNUSED */

#ifndef ATTRIBUTE_NORETURN
#define ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
#endif /* ATTRIBUTE_NORETURN */

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(m, n) __attribute__ ((__format__ (__printf__, m, n)))
#define ATTRIBUTE_PRINTF_1 ATTRIBUTE_PRINTF(1, 2)
#define ATTRIBUTE_PRINTF_2 ATTRIBUTE_PRINTF(2, 3)
#define ATTRIBUTE_PRINTF_3 ATTRIBUTE_PRINTF(3, 4)
#define ATTRIBUTE_PRINTF_4 ATTRIBUTE_PRINTF(4, 5)
#define ATTRIBUTE_PRINTF_5 ATTRIBUTE_PRINTF(5, 6)
#endif /* ATTRIBUTE_PRINTF */

#else /* __GNUC__ */

#ifndef ATTRIBUTE_MALLOC
#define ATTRIBUTE_MALLOC
#endif /* ATTRIBUTE_MALLOC */

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED
#endif /* ATTRIBUTE_UNUSED */

#ifndef ATTRIBUTE_NORETURN
#define ATTRIBUTE_NORETURN
#endif /* ATTRIBUTE_NORETURN */

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(m, n)
#define ATTRIBUTE_PRINTF_1
#define ATTRIBUTE_PRINTF_2
#define ATTRIBUTE_PRINTF_3
#define ATTRIBUTE_PRINTF_4
#define ATTRIBUTE_PRINTF_5
#endif /* ATTRIBUTE_PRINTF */

#endif /* __GNUC__ */

#endif
