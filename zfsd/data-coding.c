/* Data coding functions (encoding and decoding requests and replies).
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
#include <unistd.h>
#include <inttypes.h>
#include "data-coding.h"
#include "log.h"

/* Initialize DC to start encoding to PTR with maximal length MAX_LENGTH.  */

void
start_encoding (DC *dc, void *ptr, int max_length)
{
#ifdef ENABLE_CHECKING
  if (ptr != ALIGN_PTR_16 (ptr))
    abort ();
#endif

  dc->original = (char *) ptr;
  dc->current = (char *) ptr;
  dc->max_length = max_length;
  dc->cur_length = 0;
  encode_uint32_t (dc, 0);
}

/* Update the size of block in DC.  */

int
finish_encoding (DC *dc)
{
  *(uint32_t *) dc->original = u32_to_le ((uint32_t) dc->cur_length);

  return dc->cur_length;
}

/* Initialize DC to start decoding of PTR.  */

void
start_decoding (DC *dc, void *ptr)
{
  dc->original = (char *) ptr;
  dc->current = (char *) ptr;
  dc->max_length = 4;
  dc->cur_length = 0;
  decode_int32_t (dc, (int32_t *) &dc->max_length);
}

/* Decode a value of type T and size S from DC and store it to *RET.
   Call F to transform little endian to cpu endian.
   Return true on success.  */
#define DECODE_SIMPLE_TYPE(T, S, F)				\
int								\
decode_##T (DC *dc, T *ret)					\
{								\
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    return 0;							\
								\
  dc->current = (char *) ALIGN_PTR_##S (dc->current);		\
  *ret = F (* (T *) dc->current);				\
  dc->current += S;						\
								\
  return 1;							\
}

/* Encode a value VAL of type T and size S to DC.
   Call F to transform cpu endian to little endian.
   Return true on success.  */
#define ENCODE_SIMPLE_TYPE(T, S, F)				\
int								\
encode_##T (DC *dc, T val)					\
{								\
  int prev = dc->cur_length;					\
								\
  /* Advance and check the length.  */				\
  dc->cur_length = ALIGN_##S (dc->cur_length) + S;		\
  if (dc->cur_length > dc->max_length)				\
    {								\
      dc->cur_length = prev;					\
      return 0;							\
    }								\
								\
  dc->current = (char *) ALIGN_PTR_##S (dc->current);		\
  *(T *) dc->current = F (val);				\
  dc->current += S;						\
								\
  return 1;							\
}

DECODE_SIMPLE_TYPE (char, 1, )
DECODE_SIMPLE_TYPE (int16_t, 2, le_to_i16p)
DECODE_SIMPLE_TYPE (uint16_t, 2, le_to_u16p)
DECODE_SIMPLE_TYPE (int32_t, 4, le_to_i32p)
DECODE_SIMPLE_TYPE (uint32_t, 4, le_to_u32p)
DECODE_SIMPLE_TYPE (int64_t, 8, le_to_i64p)
DECODE_SIMPLE_TYPE (uint64_t, 8, le_to_u64p)

ENCODE_SIMPLE_TYPE (char, 1, )
ENCODE_SIMPLE_TYPE (int16_t, 2, i16_to_le)
ENCODE_SIMPLE_TYPE (uint16_t, 2, u16_to_le)
ENCODE_SIMPLE_TYPE (int32_t, 4, i32_to_le)
ENCODE_SIMPLE_TYPE (uint32_t, 4, u32_to_le)
ENCODE_SIMPLE_TYPE (int64_t, 8, i64_to_le)
ENCODE_SIMPLE_TYPE (uint64_t, 8, u32_to_le)
