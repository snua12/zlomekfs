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

#include "stdio.h"

#include "syplog.h"
#include "reader-api.h"

static char * log_settings [] = 
{
"dump_logs",
"--" PARAM_WRITER_TYPE_LONG "=" FILE_WRITER_NAME,
"--" PARAM_WRITER_FMT_LONG "=" USER_READABLE_FORMATER_NAME,
"--" PARAM_WRITER_FN_LONG "=" "/dev/tty"
};
  int log_settings_count = 4;

int main (int argc, char ** argv)
{
  struct reader_def input_parser;
  struct logger_def output_printer;
  struct log_struct_def log;

  syp_error ret_value = NOERR;

  ret_value = open_reader (&input_parser, argc, argv);
  if (ret_value != NOERR)
  {
    printf ("init fatal opening input: %d, %s\n", ret_value, syp_error_to_string (ret_value));
    exit(ret_value);
  }

  ret_value = open_log (&output_printer, "dump_node", log_settings_count, log_settings);
  if (ret_value != NOERR)
  {
    printf ("init fatal opening output: %d, %s\n", ret_value, syp_error_to_string (ret_value));
    exit(ret_value);
  }

  while (TRUE)
  {
    ret_value = read_log (&input_parser, &log);
    if (ret_value != NOERR)
    {
      printf ("reading ended: %d, %s\n", ret_value, syp_error_to_string (ret_value));
      goto FINISHING;
    }
    
    ret_value = do_log (&output_printer, LOG_ALL, FACILITY_ALL, "log read:\n");
    if (ret_value != NOERR)
    {
      printf ("log print failure: %d, %s\n", ret_value, syp_error_to_string (ret_value));
      goto FINISHING;
    }

    ret_value = write_log (&(output_printer.printer), &log);
    if (ret_value != NOERR)
    {
      printf ("writer print failure %d: %s\n", ret_value, syp_error_to_string (ret_value));
      goto FINISHING;
    }
  }

FINISHING:
  close_log (&output_printer);
  close_reader (&input_parser);
  
  if (ret_value == ERR_END_OF_LOG)
    exit (0);
  else
    exit (ret_value);
}
