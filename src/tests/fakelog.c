/*! \file
    \brief Dump logs from target.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html */

#define _GNU_SOURCE
#include <stdio.h>
#define _GNU_SOURCE

#include "syplog.h"
#include "medium-api.h"
#include "file-medium.h"

static char * log_settings [] = 
{
"dump_logs",
"--" PARAM_MEDIUM_TYPE_LONG "=" FILE_MEDIUM_NAME,
"--" PARAM_MEDIUM_FMT_LONG "=" USER_READABLE_FORMATER_NAME,
"--" PARAM_MEDIUM_FN_LONG "=" "/dev/tty",
"--" PARAM_MEDIUM_OP_LONG "=" OPERATION_WRITE_NAME
};
  int log_settings_count = 4;

int main (int argc UNUSED, char ** argv UNUSED)
{
  struct logger_def output_printer;
  char * buffer = malloc (1024 * sizeof (char));
  unsigned int buffer_size = 1024;
  int i = 0;
  FILE * sin = stdin;

  syp_error ret_value = NOERR;
  int sys_error = 0;

  ret_value = open_log (&output_printer, "fakelog", log_settings_count, log_settings);
  if (ret_value != NOERR)
  {
    printf ("init fatal opening output: %d, %s\n", ret_value, syp_error_to_string (ret_value));
    exit(ret_value);
  }

  while (TRUE)
  {
    memset (buffer, 0, 1024);
    sys_error = getline(&buffer, &buffer_size, sin);
    if (sys_error <= 0)
      break;
    
    for (i=0; i < 11; i++)
    {
      ret_value = do_log (&output_printer, i, FACILITY_LOG, "%s", buffer);
      if (ret_value != NOERR)
      {
        printf ("reading ended: %d, %s\n", ret_value, syp_error_to_string (ret_value));
        goto FINISHING;
      }
    }
    
  }

FINISHING:
  close_log (&output_printer);
  
  if (ret_value == ERR_END_OF_LOG)
    exit (0);
  else
    exit (ret_value);
}
