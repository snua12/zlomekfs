#ifndef USER_READABLE_FORMATTER_H
#define USER_READABLE_FORMATTER_H

/* ! \file \brief User readable formatter definitions.  */

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */


#include "log-struct.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/// user readable name of user readable formatter (used also as parameter 
	// when creating new formatter)
#define	USER_READABLE_FORMATTER_NAME	"user"

	/* ! Format log to socket in user readable manner @see steam_write_format
	   @see formatter-api.h */
	int32_t user_readable_stream_write(log_struct message, int socket);

	/* ! Format log to memory in user readable manner @see mem_write_format
	   @see formatter-api.h */
	int32_t user_readable_mem_write(log_struct message, void *mem_addr);

	/* ! Format log to file in user readable manner @see file_write_format
	   @see formatter-api.h */
	int32_t user_readable_file_write(log_struct message, FILE * file);


	/* ! Read log from socket in user readable manner @see steam_read_format
	   @see formatter-api.h */
	int32_t user_readable_stream_read(log_struct message, int socket);

	/* ! Read log from memory in user readable manner @see mem_read_format
	   @see formatter-api.h */
	int32_t user_readable_mem_read(log_struct message, void *mem_addr);

	/* ! Read log from file in user readable manner @see file_read_format
	   @see formatter-api.h */
	int32_t user_readable_file_read(log_struct message, FILE * file);


	/* ! Returns maximum length (in bytes) of space occupied in target medium
	   @see max_print_size @see formatter-api.h */
	int32_t user_readable_max_print_size(void);
	extern struct formatter_def user_readable_formatter;

#ifdef __cplusplus
}
#endif

#endif							/* USER_READABLE_FORMATTER_H */
