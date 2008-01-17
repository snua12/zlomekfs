/*! \file
    \brief Main generic formatter handling functions implementations.

  There are implementations of initializers, etc.
*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Syplog; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/


#include "formatter-api.h"
#include "user-readable-formatter.h"

/*! Translation table between formatter names and pointers 
  to static formatter type specific structures.
  The table must be NULL terminated.
  One record type specification.
*/
struct formatter_name
{
  /// user readable name of formatter
  char * name;
  /// pointer to structure representing given formatter type
  formatter fmt;
};

void print_formatters_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;
  
  tabize_print (tabs, fd, "formatter types:\n");
  tabs ++;
  
  tabize_print (tabs, fd, "%s - store logs \"as is\" in memory \n",
    RAW_FORMATTER_NAME);
  tabize_print (tabs +1, fd, "NOTE: this format is platform dependent \n");

  tabize_print (tabs, fd, "%s - store logs in user readable format \n",
    USER_READABLE_FORMATTER_NAME);
  tabize_print (tabs +1, fd, "(similar to /var/log/messages)\n");
  
}

/*! Translation table between formatter names and pointers. */
struct formatter_name formatter_translation_table [] = 
{
/*
  {LOG4J_FORMATTER_NAME,		log4j_formatter},
  {XML_FORMATTER_NAME,		xml_formatter},
*/
  {RAW_FORMATTER_NAME,		&raw_formatter},
  {USER_READABLE_FORMATTER_NAME,	&user_readable_formatter},
  {NULL,		NULL}
};

/// Returns formatter type description structure according to name
formatter formatter_for_name (const char * formatter_name)
{
  int table_index = 0;

  for (table_index = 0;
       formatter_translation_table[table_index].name != NULL;
       table_index ++)
  {
    if (strncmp(formatter_name, formatter_translation_table[table_index].name,
                FORMATTER_NAME_LEN) == 0)
      break;
  }

  if (table_index >= 0)
  {
    return formatter_translation_table[table_index].fmt;
  }
  else
    return NULL;
}
