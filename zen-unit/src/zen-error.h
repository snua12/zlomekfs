#ifndef		ZEN_ERROR_H
#define		ZEN_ERROR_H

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

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

// big numbers to avoid collision with std errors, 20000 > to avoid collision with syp_error range
typedef enum 
{
	ZEN_NOERR = 0,
	ZEN_ERR_INTERNAL = 20001,
	ZEN_ERR_BAD_FILE = 20002,
	ZEN_ERR_ELF = 20003,
} zen_error;

#define fail(ret,x,args...) do { printf (x, ## args); exit (ret); } while (0)

#define report_error(x,args...) printf (x, ## args)

static inline zen_error elf_to_zen_err (int err)
{
	return ZEN_ERR_ELF;
}

#ifdef __cplusplus
}
#endif

#endif		/* ZEN_ERROR_H */

