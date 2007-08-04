#ifndef			SYPLOG_H
#define			SYPLOG_H

/*! \file
    \brief Main routines function definitions.  
    
  Log opening, closing and using main functions and definitions.

  Logger is defined by
  <ul>
    <li>writer - which define how and where to write the message
                 (for example write logs <i>in user readable format</i>
                  to <i>file)</i>
                 (part of writer is <i>formater</i> which define the format)
                 Writer api is defined in writer-api.h.
    </li>
    <li>log level - in general verbosity of logger. See log-level.h for
                    log level enumeration and more elaborated description.
    </li>
    <li>facilities - rules for logging (or not logging) mesages from 
                     different parts of application.
                     For more read file facility.h.
    </li>

  </ul>
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
#include "writer-api.h"
#include "syp-error.h"
#include "facility.h"
#include "log-level.h"

/*! Structure holding logger state and configuration. */
typedef struct logger_def {
  /// writer used
  struct writer_def printer;
  /*! Bitmap of facilities to log.
    1 bit means to log facility, 0 bit means not to log messages from facility.
  */
  uint64_t facilities;
  /// Verbosity of logger - only logs with log_level lower than this will be logged.
  uint32_t log_level;
  /// cached hostname - set on opening to avoid getting on every message
  char hostname[HOSTNAME_LEN];
  /// cached name of zfs node - sets on opening to avoid getting on every message
  char node_name[NODE_NAME_LEN];
  /// cached timezone - set on opening to avoid getting on every message
  uint64_t timezone;
} * logger;

/*! Open log with given settings.
    On logger opening, the log loads timezone and hostname.
    When one of them change, logger must be reopened.
  @param glogger pointer to uninitialized logger structure to initialize (non NULL)
  @param node_name name of this node. it will be cached for use in message metadata
         (maximum length with \0 is NODE_NAME_LEN) end will be truncated
  @param argc argv arguments count
  @param argv arguments in standart "main" function format. Options for logger, writer and formater in one.
  @return std errors
*/
syp_error open_log (logger glogger, const char * node_name, int argc, char ** argv);


/*! Sends message to logger.
  @param glogger initialized logger to send message to (non NULL)
  @param level importance of message. 
  If this is bigger than logger verbosity, the message won't be logged.
  @param facility ORed bitmap of facilities to which message is connected.
  If no facility is set on in logger, the message wont' be logged.
  @param format printf-like formatted message to log 
         (whole message with inprints and \0 must fit into LOG_MESSAGE_LEN)
  @return std errors
*/
syp_error do_log (logger glogger, log_level_t level, facility_t facility, const char *format, ...);

/*! Close logger, deinitialize intrnal structures and free internal pointers.
  The structure itself won't be freed.
  @param glogger non NULL pointer to initialized logger
  @return std errors. When error is returned, the logger is in undefined state,
  but most likely internal structures are freed.
*/
syp_error close_log (logger glogger);

/*! Sets actual log level (verbosity) of logger.
  Messages with greater log level than this won't be logged
  @param glogger non NULL pointer to initialized logger structure
  @param level verbosity to set
  @return std errors
*/
syp_error set_log_level (logger glogger, log_level_t level);

/*! Gets actual log level (verbosity) of logger.
  Messages with greater log level than this won't be logged
  @param glogger non NULL pointer to initialized logger structure
  @param level non NULL pointer to where store log_level
  @return std errors
*/
syp_error get_log_level (logger glogger, log_level_t * level);

/*! Turns logging for facility on. 
  When more facilities are given, all of them will be turned on.
  @param glogger non NULL pointer to initialized logger structure
  @param facility facility (facilities) to turn on
  @return std errors
*/
syp_error set_facility (logger glogger, facility_t facility);

/*! Turns logging for facility off.
  When more facilities are given, all of them will be turned off.
  @param glogger non NULL pointer to initialized logger structure
  @param facility facility (facilities) to turn off
  @return std errors
*/
syp_error unset_facility (logger glogger, facility_t facility);

/*! Sets actual facilities bitmap with facilities which have to be logged.
  Messages with no facility in this bitmab won't be logged.
  @param glogger non NULL pointer to initialized logger structure
  @param facilities bitmap of facilities
  @return std errors
*/
syp_error set_facilities (logger glogger, facility_t facilities);

/*! Gets actual facilities bitmap with facilities which are logged.
  Messages with no facility in this bitmab won't be logged.
  @param glogger non NULL pointer to initialized logger structure
  @param facilities non NULL pointer to bitmap where to store actual facilities
  @return std errors
*/
syp_error get_facilities (logger glogger, facility_t * facilities);

/*!  Set hostname of machine running application. Use for reset it on change.
  @param glogger initialized logger structure (non NULL)
  @param hostname actual hostname of machine (max len HOSTNAME_LEN, non NULL)
  @return std errors
*/
syp_error set_hostname (logger glogger, const char * hostname);

/*!  Set timezone where application is running. Use for reset it on change.
  @param glogger initialized logger structure (non NULL)
  @param timezone local timezone
  @return std errors
*/
syp_error set_timezone (logger glogger, uint64_t timezone);

/*!  Set zfsd node name. Use for reset in on change.
  @param glogger initialized logger structure (non NULL)
  @param node_name actual node name of zfsd node (max len NODE_NAME_LEN, non NULL)
  @return std errors
*/
syp_error set_node_name (logger glogger, const char * node_name);


#endif		/*SYPLOG_H*/
