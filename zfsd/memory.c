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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "memory.h"
#include "log.h"

void *
xcalloc(size_t nmemb, size_t size)
{
  void *r = calloc(nmemb, size);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  return r;
}

void *
xmalloc(size_t size)
{
  void *r = malloc(size);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  return r;
}

void *
xrealloc(void *ptr, size_t size)
{
  void *r = realloc(ptr, size);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  return r;
}

char *
xstrdup(const char *s)
{
  char *r = strdup(s);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  return r;
}

char *
xstrndup(const char *s, size_t n)
{
  int len;
  char *r;
  
  len = strlen(s);
  if (len > n)
    len = n;
  
  r = malloc(len + 1);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  memcpy(r, s, len);
  r[len] = 0;
  return r;
}

void *
xmemdup(const void *src, size_t n)
{
  void *r = malloc(n);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }
  memcpy(r, src, n);
  return r;
}

char *
xstrconcat(int n, ...)
{
  va_list va;
  int i, len;
  char *r, *s, *d;
  int l[n];

  /* Compute the final length.  */
  len = 0;
  va_start(va, n);
  for (i = 0; i < n; i++)
    {
      s = va_arg(va, char *);
      l[i] = strlen(s);
      len += l[i];
    }
  va_end(va);

  r = malloc (len + 1);
  if (!r)
    {
      message(-1, stderr, "Not enough memory.\n");
      abort();
    }

  /* Concatenate the strings.  */
  d = r;
  va_start(va, n);
  for (i = 0; i < n; i++)
    {
      s = va_arg(va, char *);
      memcpy(d, s, l[i]);
      d += l[i];
    }
  *d = 0;
  va_end(va);

  return r;
}
