#ifndef		ZEN_ERROR_H
#define		ZEN_ERROR_H

/*! \file
    \brief Error definitions for zen-unit library.
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

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Big numbers to avoid collision with std errors, 20000 > to avoid collision with syp_error range.
  Collision should be harmless but this way it is easier to found forgotten translation.
*/
/** Enumeration of errors that could be returned by zen-unit library. */
typedef enum 
{
	/// no error, everything is o.k.
	ZEN_NOERR = 0,
	/// internal error of zen-unit (bug)
	ZEN_ERR_INTERNAL = 20001,
	/// bad file (type, corrupted, bad permissions) given to library to parse
	ZEN_ERR_BAD_FILE = 20002,
	/// general (unspecified, unknown) error of libelf library
	ZEN_ERR_ELF = 20003,
} zen_error;

/** Fail and print error to stderr.
 *
 * @param ret exit code
 * @param x message to print (printf formatting string)
 * @param args auxiliary args for printf
*/
#define fail(ret,x,args...) do { fprintf (stderr, x, ## args); exit (ret); } while (0)

/** Report library error (don't exit). Should print string to stderr.
 *
 * @param x message to print (printf formatting string)
 * @param args auxiliary args for printf
*/
#define report_error(x,args...) fprintf (stderr, x, ## args)

/** Translate libelf error to zen_error.
 *
 * @param err error returned from libelf function call
 * @return zen_error associated with given libelf error
*/
static inline zen_error elf_to_zen_err (int err)
{
	return ZEN_ERR_ELF;
}

#ifdef __cplusplus
}
#endif

#endif		/* ZEN_ERROR_H */

