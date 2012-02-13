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

#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"
#include "memory.h"
#include "varray.h"

/* ! \var string empty_string Empty string.  */
string empty_string = STRING_EMPTY_INITIALIZER;

/* ! \var string invalid_string Invalid string.  */
string invalid_string = STRING_INVALID_INITIALIZER;;

/* ! Similar to CALLOC but always returns valid pointer.  */
void *xcalloc(size_t nmemb, size_t size)
{
	void *r = calloc(nmemb, size);
	if (!r)
	{
		message(LOG_ALERT, FACILITY_MEMORY, "Not enough memory.\n");
		zfsd_abort();
	}
	return r;
}

/* ! Similar to MALLOC but always returns valid pointer.  */
void *xmalloc(size_t size)
{
	void *r = malloc(size);
	if (!r)
	{
		message(LOG_ALERT, FACILITY_MEMORY, "Not enough memory.\n");
		zfsd_abort();
	}
	return r;
}

/* ! Similar to REALLOC but always returns valid pointer.  */
void *xrealloc(void *ptr, size_t size)
{
	void *r = realloc(ptr, size);
	if (!r)
	{
		message(LOG_ALERT, FACILITY_MEMORY, "Not enough memory.\n");
		zfsd_abort();
	}
	return r;
}

/* ! Similar to STRDUP but always returns valid pointer.  */
char *xstrdup(const char *s)
{
	char *r = strdup(s);
	if (!r)
	{
		message(LOG_ALERT, FACILITY_MEMORY, "Not enough memory.\n");
		zfsd_abort();
	}
	return r;
}

/* ! Return a copy of memory SRC of size N.  */
void *xmemdup(const void *src, size_t n)
{
        void *r = xmalloc(n);
        memcpy(r, src, n);
        return r;
}

/* ! Create string DEST from S.  */
void xmkstring(string * dest, const char *s)
{
	dest->len = strlen(s);
        dest->str = (char *) xmemdup(s, dest->len + 1);
}

/* ! Frees string */
void xfreestring(string * s)
{
	if (s->str != NULL)
	{
		free(s->str);
		s->str = NULL;
		s->len = 0;
	}
}

/* ! Duplicate string SRC and store it to DEST.  SRC and DEST may be the same
   string structure.  */
void xstringdup(string * dest, string * src)
{
        dest->len = src->len;
        dest->str = (char *) xmemdup(src->str, src->len + 1);
 }

/* ! Return a concatenation of N strings.  */
char * xstrconcat(const char * s1, ...)
{
    char * r, *d;
    const char * s;
    size_t len;
    va_list va;
    va_start(va,s1);
    for (len = 0, s = s1; s != NULL; s = va_arg(va, const char *))
    {
        len += strlen(s);
    }
    va_end(va);

    r = xmalloc(len + 1);
    d = r;

    va_start(va, s1);
    for (s = s1; s != NULL; s = va_arg(va, const char *))
    {
        len = strlen(s);
        memcpy(d, s, len);
        d += len;
    }
    va_end(va);
    d[0] = 0;

    return r;
}

/* ! Return a concatenation of strings stored in varray.  */
void xstringconcat_varray(string * dst, varray * va)
{
	unsigned int i, n;
	size_t len;
	char *d;

	n = VARRAY_USED(*va);

	/* Compute the final length and lengths of all input strings.  */
	len = 0;
	for (i = 0; i < n; i++)
		len += VARRAY_ACCESS(*va, i, string).len;

	dst->len = len;
        dst->str = (char *)xmalloc(len + 1);

	/* Concatenate the strings.  */
	d = dst->str;
	for (i = 0; i < n; i++)
	{
		string *str;

		str = &VARRAY_ACCESS(*va, i, string);
		memcpy(d, str->str, str->len);
		d += str->len;
	}
	*d = 0;
}

/* ! Set a copy of SRC of length LEN to DST.  */

static void set_string_with_length(string * dst, const char *src, int len)
{
	if (dst->str)
		free(dst->str);

        dst->len = len;
        dst->str= (char *)xmemdup(src, dst->len + 1);
}

/* ! Set a copy of SRC to DST.  */

void set_str(string * dst, const char *src)
{
	set_string_with_length(dst, src, strlen(src));
}

/* ! Set a copy of SRC to DST.  */

void set_string(string * dst, string * src)
{
    set_string_with_length(dst, src->str, src->len);
}

/* ! Append STR of length LEN to SRC and store it to DST.  */

void append_string(string * dst, string * src, const char *str, unsigned int len)
{
	dst->len = src->len + len;
        dst->str = (char *)xmalloc(dst->len + 1);

	memcpy(dst->str, src->str, src->len);
	memcpy(dst->str + src->len, str, len + 1);
}

/* ! Append "/" and NAME of length LEN to PATH and store it to DST.  */

void
append_file_name(string * dst, string * path, const char *name,
				 unsigned int len)
{
	dst->len = path->len + 1 + len;
        dst->str = (char *)xmalloc(dst->len + 1);

	memcpy(dst->str, path->str, path->len);
	// TODO: ugly conversion from "/" to '/'
	dst->str[path->len] = DIRECTORY_SEPARATOR[0];
	memcpy(dst->str + path->len + 1, name, len + 1);
}

uint32_t stringlen(string * str)
{
	return str->len;
}

bool stringeq(string * str1, string * str2)
{
	if (stringlen(str1) != stringlen(str2))
		return false;

	return (strcmp(str1->str, str2->str) == 0);
}

uint32_t split_and_trim(char *line, int n, string * parts)
{
	int i;
	char *start, *colon;

	i = 0;
	while (1)
	{
		/* Skip white spaces.  */
		while (*line == ' ' || *line == '\t')
			line++;

		/* Remember the beginning of a part. */
		start = line;
		if (i < n)
			parts[i].str = start;

		/* Find the end of a part.  */
		while (*line != 0 && *line != '\n' && *line != ':')
			line++;
		colon = line;

		if (i < n)
		{
			if (line > start)
			{
				/* Delete white spaces at the end of a part.  */
				line--;
				while (line >= start && (*line == ' ' || *line == '\t'))
				{
					*line = 0;
					line--;
				}
				line++;
			}
			parts[i].len = line - start;
		}

		i++;

		if (*colon == ':')
		{
			*colon = 0;
			line = colon + 1;
		}
		else
		{
			/* We are at the end of line.  */
			*colon = 0;
			break;
		}
	}

	return i;
}

