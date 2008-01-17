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

   This file is part of Syplog.

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
typedef uint32_t		facility_t;

/// maximum length of stringified facility representation
#define FACILITY_STRING_LEN	32

/// fallback facility (default)
#define	FACILITY_UNKNOWN        0x0
#define	FACILITY_UNKNOWN_NAME		"UNKNOWN/NONTRIVIAL"
/// log message apply on logging facility
#define	FACILITY_LOG            0x1
#define	FACILITY_LOG_NAME		"LOG"
/// log message apply on threading
#define	FACILITY_THREADING      0x2
#define	FACILITY_THREADING_NAME		"THREADING"
/// log message apply on networking
#define	FACILITY_NET            0x4
#define	FACILITY_NET_NAME		"NET"
/// log message apply on caching
#define	FACILITY_CACHE          0x8
#define	FACILITY_CACHE_NAME		"CACHE"
/// log message apply on data handling
#define	FACILITY_DATA           0x10
#define	FACILITY_DATA_NAME		"DATA"
/// log message apply on memory
#define	FACILITY_MEMORY         0x20
#define	FACILITY_MEMORY_NAME		"MEMORY"
/// log message apply on configuration
#define	FACILITY_CONFIG         0x40
#define	FACILITY_CONFIG_NAME		"CONFIG"

/// log message apply on dbus
#define FACILITY_DBUS           0x80
#define FACILITY_DBUS_NAME		"DBUS"
/// facility for global testing
#define FACILITY_ZFSD		0x100
#define	FACILITY_ZFSD_NAME		"ZFSD"

/// do not log messages from any facility
#define	FACILITY_NOTHING        0x0
#define	FACILITY_NOTHING_NAME		"NOTHING"
/// log messages from all facilities
#define	FACILITY_ALL            (uint32_t)-1
#define	FACILITY_ALL_NAME		"ALL"


// TODO: multifacilities
/*! Translates singular facility to name
  @param facility singular facility. Multiple ORed facilities leads to unknown / nontrivial response
  @return user readable string representing given facility or NULL
*/
static inline const char * facility_to_name (facility_t facility)
{
  if ((facility & FACILITY_LOG) > 0)
    return FACILITY_LOG_NAME;
  if ((facility & FACILITY_THREADING) > 0)
    return FACILITY_THREADING_NAME;
  if ((facility & FACILITY_NET) > 0)
    return FACILITY_NET_NAME;
  if ((facility & FACILITY_CACHE) > 0)
    return FACILITY_CACHE_NAME;
  if ((facility & FACILITY_DATA) > 0)
    return FACILITY_DATA_NAME;
  if ((facility & FACILITY_MEMORY) > 0)
    return FACILITY_MEMORY_NAME;
  if ((facility & FACILITY_CONFIG) > 0)
    return FACILITY_CONFIG_NAME;
  if ((facility & FACILITY_DBUS) > 0)
    return FACILITY_DBUS_NAME;
  if ((facility & FACILITY_ZFSD) > 0)
    return FACILITY_ZFSD_NAME;

  if (facility == FACILITY_NOTHING)
    return FACILITY_NOTHING_NAME;
  if (facility == FACILITY_ALL)
    return FACILITY_ALL_NAME;

  return FACILITY_UNKNOWN_NAME;
}

// TODO: multifacilities
/*! Translates singular facility name to bit representation
  @param facility_name singular facility name
  @return bit representation fo facility or FACILITY_UNKNOWN
*/
static inline facility_t facility_from_string (const char * facility_name)
{
  if (strncmp (facility_name, FACILITY_LOG_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_LOG;
  if (strncmp (facility_name, FACILITY_THREADING_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_THREADING;
  if (strncmp (facility_name, FACILITY_NET_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_NET;
  if (strncmp (facility_name, FACILITY_CACHE_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_CACHE;
  if (strncmp (facility_name, FACILITY_DATA_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_DATA;
  if (strncmp (facility_name, FACILITY_MEMORY_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_MEMORY;
  if (strncmp (facility_name, FACILITY_CONFIG_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_CONFIG;
  if (strncmp (facility_name, FACILITY_DBUS_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_DBUS;
  if (strncmp (facility_name, FACILITY_ZFSD_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_ZFSD;

  if (strncmp (facility_name, FACILITY_NOTHING_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_NOTHING;
  if (strncmp (facility_name, FACILITY_ALL_NAME, FACILITY_STRING_LEN) == 0)
    return FACILITY_ALL;

  return FACILITY_UNKNOWN;
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
