/* ! \file \brief Variable-sized array datatype.  */

/* Copyright (C) 2003 Josef Zlomek

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
#include "varray.h"
#include <stdlib.h>
#include "log.h"
#include "memory.h"

/* ! Initialize a variable-sized array *VA to NELEM elements of size SIZE.  */

void varray_create(varray * va, unsigned int size, unsigned int nelem)
{
	va->nelem = nelem;
	va->nused = 0;
	va->size = size;
	va->array = xmalloc(size * nelem);
}

void varray_destroy(varray * va)
{
	free(va->array);
	va->nelem = 0;
	va->nused = 0;
	va->size = 0;
	va->array = NULL;
}

void varray_grow(varray * va, unsigned int nelem)
{
#ifdef ENABLE_CHECKING
	if (va->size == 0 || va->array == NULL)
		abort();
#endif

	va->array = xrealloc(va->array, va->size * nelem);
	va->nelem = nelem;
}

#ifdef ENABLE_CHECKING

/* ! Report an internal error "Element out of bounds".  */

void varray_check_failed(unsigned int pos, const char *file, int line)
{
	internal_error("Element %u out of bounds, at %s:%d", pos, file, line);
}

#endif
