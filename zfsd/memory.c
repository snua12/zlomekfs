/* Memory management functions.
   Copyright (C) 2003 Josef Zlomek

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
  char *s;
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

  return xstrconcat_varray (&v);
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

/* Set *DESTP to a new string SRC whose length is LENGTH.  */

void
set_string_with_length (char **destp, const char *src, int len)
{
  if (*destp)
    free (*destp);

  *destp = (char *) xmemdup (src, len + 1);
}

/* Set *DESTP to a new string SRC.  */

void
set_string (char **destp, const char *src)
{
  set_string_with_length (destp, src, strlen (src));
}
