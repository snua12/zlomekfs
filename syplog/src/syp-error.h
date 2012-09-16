#ifndef SYP_ERROR_H
#define SYP_ERROR_H

/* ! \file \brief Logger specific errors and helper functions.

   Errors are enumerated int with predefined values.

   Use bigger number to avoid missinterpretation as system error. Please add
   string description to the error in syp_error_to_string too when adding an
   error. */

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


#ifdef __cplusplus
extern "C"
{
#endif

#define	SYS_NOERR	0

	/* ! Enumeration of errors which could arise in logger */
	typedef enum
	{
		/// no error at all
		NOERR = 0,
		/// bad params given to function
		ERR_BAD_PARAMS = 10001,
		/// file can't be opened
		ERR_FILE_OPEN = 10002,
		/// end of file reached
		ERR_END_OF_LOG = 10003,
		/// functionality not implemented
		ERR_NOT_IMPLEMENTED = 10004,
		/// unspecified error from operating system
		ERR_SYSTEM = 10005,
		/// try to do operation on uninitialized component
		ERR_NOT_INITIALIZED = 10006,
		/// data truncated in operation
		ERR_TRUNCATED = 10007,
		/// bad message type received
		ERR_BAD_MESSAGE = 10008,
		/// dbus communication error
		ERR_DBUS = 10009,
		/// out of memory
		ERR_NO_MEMORY = 10010,

	} syp_error;

	/* ! Returns user readable description of error @param error error occured
	   @return static string description of error */
	char *syp_error_to_string(syp_error error);

	/* ! Translates system error to syplog errors @param sys_error system
	   error @return syp_error equivalent of system_error or ERR_SYSTEM when
	   unknown */
	syp_error sys_to_syp_error(int sys_error);

#ifdef __cplusplus
}
#endif

#endif							/* SYP_ERROR_H */
