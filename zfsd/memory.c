/* Memory management functions.
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

#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"
#include "memory.h"
#include "varray.h"

string empty_string = { 0, "" };

/* Similar to CALLOC but always returns valid pointer.  */
void *
xcalloc (size_t nmemb, size_t size)
{
  void *r = calloc (nmemb, size);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  return r;
}

/* Similar to MALLOC but always returns valid pointer.  */
void *
xmalloc (size_t size)
{
  void *r = malloc (size);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  return r;
}

/* Similar to REALLOC but always returns valid pointer.  */
void *
xrealloc (void *ptr, size_t size)
{
  void *r = realloc (ptr, size);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  return r;
}

/* Similar to STRDUP but always returns valid pointer.  */
char *
xstrdup (const char *s)
{
  char *r = strdup (s);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  return r;
}

/* Similar to STRNDUP but always returns valid pointer.  */
char *
xstrndup (const char *s, size_t n)
{
  size_t len;
  char *r;

  len = strlen (s);
  if (len > n)
    len = n;

  r = (char *) malloc (len + 1);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  memcpy (r, s, len);
  r[len] = 0;
  return r;
}

/* Create string DEST from S.  */
void
xmkstring (string *dest, const char *s)
{
  dest->len = strlen (s);
  dest->str = (char *) malloc (dest->len + 1);
  if (!dest->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  memcpy (dest->str, s, dest->len + 1);
}

/* Duplicate string SRC and store it to DEST.  SRC and DEST may be the same
   string structure.  */
void
xstringdup (string *dest, string *src)
{
  char *old_str;

  old_str = src->str;
  dest->len = src->len;
  dest->str = (char *) malloc (dest->len + 1);
  if (!dest->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  memcpy (dest->str, old_str, dest->len + 1);
}

/* Return a copy of memory SRC of size N.  */
void *
xmemdup (const void *src, size_t n)
{
  void *r = malloc (n);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  memcpy (r, src, n);
  return r;
}

/* Return a concatenation of N strings.  */
char *
xstrconcat (unsigned int n, ...)
{
  va_list va;
  unsigned int i;
  char *r, *s;
  varray v;

  /* Store the arguments to varray V.  */
  varray_create (&v, sizeof (char *), n);
  va_start (va, n);
  for (i = 0; i < n; i++)
    {
      s = va_arg (va, char *);
      VARRAY_PUSH (v, s, char *);
    }
  va_end (va);
  r = xstrconcat_varray (&v);
  varray_destroy (&v);

  return r;
}

/* Return a concatenation of strings stored in varray.  */
char *
xstrconcat_varray (varray *va)
{
  varray v;
  unsigned int i, n;
  size_t l, len;
  char *r, *s, *d;

  n = VARRAY_USED (*va);

  /* Compute the final length and lengths of all input strings.  */
  len = 0;
  varray_create (&v, sizeof (size_t), n);
  for (i = 0; i < n; i++)
    {
#ifdef ENABLE_CHECKING
      if (VARRAY_ACCESS (*va, i, char *) == NULL)
	abort ();
#endif
      l = strlen (VARRAY_ACCESS (*va, i, char *));
      VARRAY_PUSH (v, l, size_t);
      len += l;
    }

  r = (char *) malloc (len + 1);
  if (!r)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }

  /* Concatenate the strings.  */
  d = r;
  for (i = 0; i < n; i++)
    {
      s = VARRAY_ACCESS (*va, i, char *);
      l = VARRAY_ACCESS (v, i, size_t);
      memcpy (d, s, l);
      d += l;
    }
  *d = 0;
  varray_destroy (&v);

  return r;
}

/* Return a concatenation of strings stored in varray.  */
void
xstringconcat_varray (string *dst, varray *va)
{
  unsigned int i, n;
  size_t len;
  char *d;

  n = VARRAY_USED (*va);

  /* Compute the final length and lengths of all input strings.  */
  len = 0;
  for (i = 0; i < n; i++)
    len += VARRAY_ACCESS (*va, i, string).len;

  dst->len = len;
  dst->str = (char *) malloc (len + 1);
  if (!dst->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }

  /* Concatenate the strings.  */
  d = dst->str;
  for (i = 0; i < n; i++)
    {
      string *str;

      str = &VARRAY_ACCESS (*va, i, string);
      memcpy (d, str->str, str->len);
      d += str->len;
    }
  *d = 0;
}

/* Set *DESTP to a new string SRC whose length is LENGTH.  */

void
set_str_with_length (char **destp, const char *src, int len)
{
  if (*destp)
    free (*destp);

  *destp = (char *) xmemdup (src, len + 1);
}

/* Set *DESTP to a new string SRC.  */

void
set_str (char **destp, const char *src)
{
  set_str_with_length (destp, src, strlen (src));
}

/* Set a copy of SRC to DST.  */

void
set_string (string *dst, const char *src)
{
  if (dst->str)
    free (dst->str);

  dst->len = strlen (src);
  dst->str = (char *) malloc (dst->len + 1);
  if (!dst->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }
  memcpy (dst->str, src, dst->len + 1);
}

/* Append STR of length LEN to SRC and store it to DST.  */

void
append_string (string *dst, string *src, const char *str, unsigned int len)
{
  dst->len = src->len + len;
  dst->str = (char *) malloc (dst->len + 1);
  if (!dst->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }

  memcpy (dst->str, src->str, src->len);
  memcpy (dst->str + src->len, str, len + 1);
}

/* Append "/" and NAME of length LEN to PATH and store it to DST.  */ 

void
append_file_name (string *dst, string *path, const char *name, unsigned int len)
{
  dst->len = path->len + 1 + len;
  dst->str = (char *) malloc (dst->len + 1);
  if (!dst->str)
    {
      message (-1, stderr, "Not enough memory.\n");
      abort ();
    }

  memcpy (dst->str, path->str, path->len);
  dst->str[path->len] = '/';
  memcpy (dst->str + path->len + 1, name, len + 1);
}
