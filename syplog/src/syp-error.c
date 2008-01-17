/*! \file
    \brief Logger specific helper functions.

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

#include "syp-error.h"

/// Translates error number to user readable string
char * syp_error_to_string (syp_error error)
{
  switch (error)
  {
    case NOERR:
      return "No error at all";
    case ERR_BAD_PARAMS:
      return "Bad params given to function";
    case ERR_FILE_OPEN:
      return "File can't be opened";
    case ERR_END_OF_LOG:
      return "End of log reached";
    case ERR_NOT_IMPLEMENTED:
      return "Functionality not implemented";
    case ERR_SYSTEM:
      return "General error from operating system";
    case ERR_NOT_INITIALIZED:
      return "Component not initialized";
    case ERR_TRUNCATED:
      return "Data truncated in operation";
    case ERR_BAD_MESSAGE:
      return "Bad message type received";
    case ERR_DBUS:
      return "Error in communication with dbus";
    case ERR_NO_MEMORY:
      return "Out of memory";
    default:
      return "Unknown error";
  }

  return "Unknown error";
}

syp_error sys_to_syp_error (int sys_error)
{
  switch (sys_error)
  {
    //TODO: add more errors
    default:
      return ERR_SYSTEM;
  }

  return ERR_SYSTEM;
}
