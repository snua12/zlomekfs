/* ! \file \brief Memory management functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef MEMORY_H
#define MEMORY_H

#include "system.h"

# include <stddef.h>
# include "varray.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/* ! \brief String type.  */
	typedef struct string_def
	{
		uint32_t len;			/* !< Length of the string.  */
		char *str;				/* !< The string itself.  */
	} string;

	extern string empty_string;
	extern string invalid_string;

	extern void *xcalloc(size_t nmemb, size_t size) ATTRIBUTE_MALLOC;
	extern void *xmalloc(size_t size) ATTRIBUTE_MALLOC;
	extern void *xrealloc(void *ptr, size_t size) ATTRIBUTE_MALLOC;
	extern char *xstrdup(const char *s) ATTRIBUTE_MALLOC;
	extern char *xstrndup(const char *s, size_t n) ATTRIBUTE_MALLOC;
	extern void xmkstring(string * dest, const char *s);
	extern void xstringdup(string * dest, string * src);
	extern void *xmemdup(const void *src, size_t n) ATTRIBUTE_MALLOC;
	extern char *xstrconcat(unsigned int n, ...) ATTRIBUTE_MALLOC;
	extern char *xstrconcat_varray(varray * va) ATTRIBUTE_MALLOC;
	extern void xstringconcat_varray(string * dst, varray * va);
	extern void set_str(string * dst, const char *src);
	extern void set_string(string * dst, string * src);
	extern void set_string_with_length(string * dst, const char *src, int len);
	extern void append_string(string * dst, string * src, const char *str,
							  unsigned int len);
	extern void append_file_name(string * dst, string * path, const char *name,
								 unsigned int len);

#ifdef __cplusplus
}
#endif

#endif
