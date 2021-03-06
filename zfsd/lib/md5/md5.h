/*! \file \brief MD5 algorithm implementation.  */

/* This code implements the MD5 message-digest algorithm. The algorithm is due 
   to Ron Rivest.  This code was written by Colin Plumb in 1993, no copyright
   is claimed. This code is in the public domain; do with it what you wish.

   Some modifications for oftpd and ZFS: Copyright (C) 2001, 2003, 2004 Josef
   Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#ifndef MD5_H
#define MD5_H

#include "system.h"

#ifdef __cplusplus
extern "C"
{
#endif

	// TODO: use openssl md5 instead of this one

	/*! Size of MD5 hash.  */
#define MD5_SIZE 16

	/*! \brief MD5 context used while computing MD5 sum. */
	typedef struct MD5Context_def
	{
		uint32_t buf[4];
		uint32_t bits[2];
		unsigned char in[64];
	} MD5Context;

	extern void MD5Init(MD5Context * ctx);
	extern void MD5Update(MD5Context * ctx, unsigned char const *buf,
						  unsigned int len);
	extern void MD5Final(unsigned char digest[MD5_SIZE], MD5Context * ctx);
	extern void MD5HexFinal(unsigned char digest[MD5_SIZE * 2],
							MD5Context * ctx);

#ifdef __cplusplus
}
#endif

#endif							/*!MD5_H */
