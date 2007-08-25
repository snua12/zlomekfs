#ifndef WRITER_API_H
#define WRITER_API_H

/*! \file
    \brief Api for writer functions.  

  Writer is set of functions and settings describing the method
  how and where the log should be written.

  Writer state is dependent on type (file, memory, socket writer).
  Writer uses formaters to print logs into media.
 
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

#include "writer.h"

#include "file-writer.h"

#include "formater-api.h"

#define PARAM_WRITER_FMT_LONG	"formater"
#define PARAM_WRITER_FMT_CHAR	'f'

#define PARAM_WRITER_LS_LONG	"size"
#define PARAM_WRITER_LS_CHAR	's'

#define	PARAM_WRITER_TYPE_LONG	"writer"
#define	PARAM_WRITER_TYPE_CHAR	'w'

/*! Initializes writer according to parameters.
  @param argv the same format as in "main", but parses only writer specific options
  @param argc argv item count
  @param target non NULL pointer to writer structure to initialize
  @return std errors. When error is returned, structure is in undefined state
          but internal pointers are freed
*/
syp_error open_writer (struct writer_def * target, int argc, char ** argv);

/*! Close writer and free internal pointers.
  @param target non NULL pointer to initialized writer
  @return std errors. When error is returned, structure is in undefined state
          but most likeli internal pointers are freed
*/
syp_error close_writer (struct writer_def * target);

/*! Write message to log.
  @param target initialized writer (non NULL)
  @param log message to write (non NULL)
  @return std errors. On error, message may not be written.
*/
syp_error write_log (struct writer_def * target, log_struct log);



#endif /*WRITER_API_H*/
