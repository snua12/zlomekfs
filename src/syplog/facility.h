#ifndef FACILITY_H
#define FACILITY_H

/*! \file
    \brief Facilities and associated helper functions.  

  Facility is a part (or concern) of application 
  which needs to be distinguished in logging.

  Current model of facilities is a bitmap. When the bit for particular
  facility is set in bitmap, it means that messages from this facility
  have to be logged. 

  Message can be associated to more than one facility. Then it is logged
  if at least one of them is set to be logged.

*/

/* Copyright (C) 2007 Jiri Zouhar

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

#include "log-constants.h"

/// typedef for facility
typedef uint64_t		facility_t;

/// fallback facility (default)
#define	FACILITY_UNKNOWN        0x0
/// log message apply on logging facility
#define	FACILITY_LOG            0x1
/// log message apply on threading
#define	FACILITY_THREADING      0x2
/// log message apply on networking
#define	FACILITY_NET            0x4
/// log message apply on caching
#define	FACILITY_CACHE          0x8
/// log message apply on data handling
#define	FACILITY_DATA           0x10

/// facility for global testing
#define FACILITY_ZFSD		0x10000

/// do not log messages from any facility
#define	FACILITY_NOTHING        0x0
/// log messages from all facilities
#define	FACILITY_ALL            (size_t)-1L


// TODO: multifacilities
/*! Translates singular facility to name
  @param facility singular facility. Multiple ORed facilities leads to unknown / nontrivial response
  @return user readable string representing given facility or NULL
*/
static inline const char * facility_to_name (facility_t facility)
{
  if ((facility & FACILITY_LOG) > 1)
    return "LOG";
  if ((facility & FACILITY_THREADING) > 1)
    return "THREADING";
  if ((facility & FACILITY_NET) > 1)
    return "NET";
  if ((facility & FACILITY_CACHE) > 1)
    return "CACHE";
  if ((facility & FACILITY_DATA) > 1)
    return "DATA";

  if (facility == FACILITY_NOTHING)
    return "NOTHING";
  if (facility == FACILITY_ALL)
    return "ALL";

  return "UNKNOWN/NONTRIVIAL";
}

/// Turn facility on.
/*! Add (turn on) facility to bitmap
  @param bitmap initial bitmap
  @param facility one or more facilities to add to bitmap
  @return new bitmap with added facility (facilities)
*/
static inline facility_t facility_add (facility_t bitmap, facility_t facility)
{
  return bitmap | facility;
}

/// Turn facility off.
/*! Delete (turn off) facility from bitmap
  @param bitmap initial bitmap
  @param facility one or more facilities to delete from bitmap
  @return new bitmap with deleted facilities (facilities)
*/
static inline facility_t facility_del (facility_t bitmap, facility_t facility)
{
  return bitmap & ~facility;
}

/// Gets state of facility.
/*! Gets state of facility in bitmap (if has to be logged or not)
  @param bitmap bitmap
  @param facility one or more facilities to test if set in bitmap
  @return TRUE if one or more of facilities matches, FALSE if none
*/
static inline bool_t facility_get_state (facility_t bitmap, facility_t facility)
{
  if ((bitmap & facility) > 0)
    return TRUE;
  else
    return FALSE;
}

#endif /*FACILITY_H*/
