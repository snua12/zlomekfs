#ifndef RAW_FORMATER_H
#define RAW_FORMATER_H

/*! \file
    \brief Raw formater definitions.  */

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

/// user readable name of raw formater (used also as parameter when creating new formater)
#define	RAW_FORMATER_NAME	"raw"

/*! Format raw log to socket 
  @see steam_write_format 
  @see formater-api.h
*/
int32_t raw_stream_write (log_struct message, int socket);

/*! Format raw log to memory 
  @see mem_write_format 
  @see formater-api.h
*/
int32_t raw_mem_write (log_struct message, void * mem_addr);

/*! Format raw log to file
  @see file_write_format 
  @see formater-api.h
*/
int32_t raw_file_write (log_struct message, FILE * file);


/*! Read raw log from socket 
  @see steam_read_format 
  @see formater-api.h
*/
int32_t raw_stream_read (log_struct message, int socket);

/*! Read raw log from memory 
  @see mem_read_format 
  @see formater-api.h
*/
int32_t raw_mem_read (log_struct message, void * mem_addr);

/*! Read raw log from file
  @see file_read_format 
  @see formater-api.h
*/
int32_t raw_file_read (log_struct message, FILE * file);


/*! Returns maximum length (in bytes) of space occupied in target medium
  @see max_print_size 
  @see formater-api.h
*/
int32_t raw_max_print_size (void);

extern struct formater_def raw_formater;

#endif /*RAW_FORMATER_H*/
