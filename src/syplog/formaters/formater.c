/*! \file
    \brief Main generic formater handling functions implementations.

  There are implementations of initializers, etc.
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

#include "formater-api.h"
#include "user-readable-formater.h"

/*! Translation table between formater names and pointers 
  to static formater type specific structures.
  The table must be NULL terminated.
  One record type specification.
*/
struct formater_name
{
  /// user readable name of formater
  char * name;
  /// pointer to structure representing given formater type
  formater fmt;
};

void print_formaters_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;
  
  tabize_print (tabs, fd, "formater types:\n");
  tabs ++;
  
  tabize_print (tabs, fd, "%s - store logs \"as is\" in memory \n",
    RAW_FORMATER_NAME);
  tabize_print (tabs +1, fd, "NOTE: this format is platform dependent \n");

  tabize_print (tabs, fd, "%s - store logs in user readable format \n",
    USER_READABLE_FORMATER_NAME);
  tabize_print (tabs +1, fd, "(similar to /var/log/messages)\n");
  
}

/*! Translation table between formater names and pointers. */
struct formater_name formater_translation_table [] = 
{
/*
  {LOG4J_FORMATER_NAME,		log4j_formater},
  {XML_FORMATER_NAME,		xml_formater},
*/
  {RAW_FORMATER_NAME,		&raw_formater},
  {USER_READABLE_FORMATER_NAME,	&user_readable_formater},
  {NULL,		NULL}
};

/// Returns formater type description structure according to name
formater formater_for_name (const char * formater_name)
{
  int table_index = 0;

  for (table_index = 0;
       formater_translation_table[table_index].name != NULL;
       table_index ++)
  {
    if (strncmp(formater_name, formater_translation_table[table_index].name,
                FORMATER_NAME_LEN) == 0)
      break;
  }

  if (table_index >= 0)
  {
    return formater_translation_table[table_index].fmt;
  }
  else
    return NULL;
}
