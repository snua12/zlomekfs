#ifndef		ZEN_UNIT_H
#define		ZEN_UNIT_H

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

/*! \file
    \brief Interface for zen-unit

  Zen-unit is minimalistic approach to unit testing.

  Interface is as simple as two macros and there is no need to list tests somewhere.

*/

/*! \mainpage Zen-unit: Minimalistic approach to unit testing

 \section Features

   \li Written in C, C++ compatible
   \li Write tests everywhere and don't care about collecting.
   \li Very simple API

 \section Usage

    \li include zen-unit.h

 \code
#include <zen-unit.h>
 \endcode

    \li write tests everywhere in source code

 \code
ZEN_TEST(my_first_test) {
  do something
  ZEN_ASSERT(1==2,"This test should failed until math broke");

  return TRUE
}
 \endcode

    \li build your project - don't strip it

 \code
make all
 \endcode

    \li run tests on your binary

 \code
LD_PRELOAD=libzenunit.so my_binary
 \endcode

    \li output should look like:

 \code
[bug@bot src]# LD_PRELOAD= bzenunit.so my_binary
zen_formatter_to_name_test      PASS(0)
 \endcode

    \li to test shared library use following (output will be equivalent)

 \code
[root@bug src]# LD_PRELOAD=libmylib.so zenunit
zen_formatter_to_name_test      PASS(0)
 \endcode

 \see zen-unit.h

*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Zen Unit.

   Zen-unit is free software; you can redistribute it and/or modify it
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

#ifndef PASS
/// return code from test which passed
#define	PASS	0
#endif

#ifndef FAIL
/// return code from test which failed
#define FAIL	1
#endif

/// Assert statement to use in tests
/** Assertion macro - use for assumption testing
 *
 * @param test logical expression - must evaluate to 0 (failed) or 1 (assumption holds)
 * @param message textual message (zero termiated string) which has to be printed when failed (one-liner)
 * @return if assertion fails it make the test function return with FALSE
*/
#define ZEN_ASSERT(test,message) do { if (!(test)) { fprintf(stderr,  "%s:\t%s\n", __func__, message); return FAIL; } } while (0)

#define C_IDENTIFIER_CHARS	"-_a-zA-Z0-9"

#define ZEN_NAME_REGEX		"zen_"  "[" C_IDENTIFIER_CHARS "]+"  "_test"

/// Test header generator
/** Test header macro. use like function deffinition
    for example simpliest test should look like
    ZEN_TEST (test_name) { return TRUE; }
 *
 * the void * param passed to function is just for forward compatibility.
   NULL will be passed
 *
 * @param name test name (identifier) - just like identifier, no string forms
 * @return should return FALSE if failed and TRUE if successed
*/
#define ZEN_TEST(name)		int zen_ ## name ## _test (void * param __attribute__ ((__unused__)))

typedef int (*zen_test_template) (void *);

#ifdef __cplusplus
}
#endif


#endif		/* ZEN_UNIT_H */
