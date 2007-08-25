#ifndef READER_API_H
#define READER_API_H

/*! \file
    \brief Api for reader functions.  

  Reader is set of functions and settings describing the method
  how and from where to read logs;

  Reader state is dependent on type (file, memory, socket reader).
  Reader uses formaters to parse logs from media
 
  @see formater-api.h
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

#include "reader.h"

#include "file-reader.h"

#include "formater-api.h"

#define PARAM_READER_FMT_LONG	"formater"
#define PARAM_READER_FMT_CHAR	'f'

#define	PARAM_READER_TYPE_LONG	"reader"
#define	PARAM_READER_TYPE_CHAR	'r'

/*! Initializes reader according to parameters.
  @param argv the same format as in "main", but parses only reader specific options
  @param argc argv item count
  @param target non NULL pointer to reader structure to initialize
  @return std errors. When error is returned, structure is in undefined state
          but internal pointers are freed
*/
syp_error open_reader (struct reader_def * target, int argc, char ** argv);

/*! Close reader and free internal pointers.
  @param target non NULL pointer to initialized reader
  @return std errors. When error is returned, structure is in undefined state
          but most likeli internal pointers are freed
*/
syp_error close_reader (struct reader_def * target);

/*! Read message to log structure.
  @param target initialized reader (non NULL)
  @param log message to fill (non NULL)
  @return std errors. On error, message may not be written.
*/
syp_error read_log (struct reader_def * target, log_struct log);



#endif /*READER_API_H*/
