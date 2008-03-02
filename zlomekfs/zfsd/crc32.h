/*! \file
    \brief Definitions for 32-bit CRC.  */

/* Copyright (C) 2003 Josef Zlomek

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

#ifndef CRC32_H
#define CRC32_H

#include "system.h"
#include <stddef.h>

/*! CRC32 iterator. */
#define CRC32_NEXT(CRC, STR)					\
   (crc32_table[((CRC) ^ *(STR)++) & 0xff] ^ ((CRC) >> 8))

/*! Table used for computing 32-bit CRC.  */
extern const unsigned int crc32_table[256];

extern unsigned int crc32_update (unsigned int crc, const void *buf, size_t len);
extern unsigned int crc32_buffer (const void *buf, size_t len);
extern unsigned int crc32_string (const char *s);

#endif
