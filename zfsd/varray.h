/* Variable-sized array datatype.
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

#ifndef VARRAY_H
#define VARRAY_H

#include "system.h"

/* Definition of the variable-sized array.  */
typedef struct varray_def
{
  /* Total number of elements allocated.  */
  unsigned int nelem;

  /* The number of elements used.  */
  unsigned int nused;

  /* Size of an element.  */
  unsigned int size;

  /* The array itself.  */
  void *array;
} varray;

/* Total (allocated) size of variable-sized array VA.  */
#define VARRAY_SIZE(VA) ((VA).nelem)

/* Number of used elements of variable-sized array VA.  */
#define VARRAY_USED(VA) ((VA).nused)

/* Push an element X of type T into variable-sized array VA.  */
#define VARRAY_PUSH(VA, X, T)				\
  do							\
    {							\
      if ((VA).nused >= (VA).nelem)			\
	varray_grow (&(VA), 2 * (VA).nelem);		\
      ((T *) (VA).array)[(VA).nused++] = (X);		\
    }							\
  while (0)

/* Delete the last element from variable-sized array VA.  */
#define VARRAY_POP(VA) ((VA).nused--)

/* Access the top element of variable-sized array VA.  */
#define VARRAY_TOP(VA, T) (((T *) (VA).array)[(VA).nused - 1])

/* Empty the variable-sized array VA.  */
#define VARRAY_CLEAR(VA) ((VA).nused = 0)

#ifdef ENABLE_CHECKING

/* Access the Nth element of type T from variable-sized array VA with bounds
   checking.  */
#define VARRAY_ACCESS(VA, N, T)						  \
  ((N) >= (VA).nused							  \
   ? varray_check_failed ((N), __FILE__, __LINE__), *(T *) NULL \
   : ((T *) (VA).array)[N])

#else

/* Access the Nth element of type T from variable-sized array VA.  */
#define VARRAY_ACCESS(VA, N, T)						  \
   (((T *) (VA).array)[N])

#endif

extern void varray_create (varray *va, unsigned int size, unsigned int nelem);
extern void varray_destroy (varray *va);
extern void varray_grow (varray *va, unsigned int nelem);
#ifdef ENABLE_CHECKING
extern void varray_check_failed (unsigned int index, const char *file,
				 int line) ATTRIBUTE_NORETURN;
#endif

#endif
