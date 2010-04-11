/*! \file
    \brief Logging functions.  */

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

#include "log.h"
#include "system.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>
#include <execinfo.h>

#include "node.h"
#include "syplog.h"
#include "control/listener.h"
#include "pthread-wrapper.h"


struct logger_def syplogger;
struct listener_def control;


int dbus_add_log_name (DBusConnection * connection, 
                       DBusError * err_struct)
{
  syp_error ret = dbus_add_syplog_name (connection, err_struct, &syplogger);
  if (ret == NOERR)
    return TRUE;
  else
    return FALSE;
}


int dbus_release_log_name (DBusConnection * connection, 
                           DBusError * err_struct)
{
  syp_error ret = dbus_release_syplog_name (connection, err_struct, &syplogger);
  if (ret == NOERR)
    return TRUE;
  else
    return FALSE;
}

message_handle_state_e dbus_handle_log_message (DBusConnection * connection, 
                                                DBusError * err_struct,
                                                DBusMessage * msg)
{
  syp_error ret = dbus_handle_syplog_message (connection, err_struct, msg, &syplogger);
  switch (ret)
  {
    case NOERR:
      return ZFSD_MESSAGE_HANDLED;
      break;
    case ERR_BAD_MESSAGE:
      return ZFSD_MESSAGE_UNKNOWN;
      break;
    default:
      return ZFSD_HANDLE_ERROR;
  }

  return ZFSD_HANDLE_ERROR;

}


void zfs_openlog(int  argc, const char ** argv)
{
  syp_error ret_code = open_log (&syplogger, "UNDEF", argc, argv);
  if (ret_code != NOERR)
  {
    printf ("Bad params for logger initialization %d: %s\n", ret_code,
            syp_error_to_string (ret_code));

    ret_code = open_log (&syplogger, "UNDEF", 0, NULL);
  }

  if (ret_code != NOERR)
  {
    printf ("could not initialize logger %d: %s\n", ret_code,
            syp_error_to_string (ret_code));
    return;
  }

  if(ret_code != NOERR)
  {
    message(LOG_WARNING,FACILITY_CONFIG | FACILITY_LOG, "Can't initialize listen loop %d:%s\n",
      ret_code, syp_error_to_string(ret_code));
  }
}

void update_node_name (void)
{
  syp_error ret_code = NOERR;
  if (node_name.str != NULL)
    ret_code = set_node_name ( &syplogger, node_name.str);
  else
    ret_code = set_node_name (&syplogger, "UNDEF");

  if (ret_code != NOERR)
    message (LOG_WARNING, FACILITY_LOG, "could not set node_name %d: %s\n", ret_code,
             syp_error_to_string (ret_code));
}

void zfs_closelog(void)
{
  close_log (&syplogger);
}

/*! Print stack */
void show_stackframe(void) {
  void *trace[16];
  char **messages = (char **)NULL;
  int i, trace_size = 0;

  trace_size = backtrace(trace, 16);
  messages = backtrace_symbols(trace, trace_size);
  printf("[bt] Execution path:\n");
  for (i=0; i<trace_size; ++i)
    printf("[bt] %s\n", messages[i]);
  free (messages);
}

/*! Print the internal error message and exit.  */
void
internal_error (const char *format, ...)
{
  va_list va;
  char msg[1024];
#ifdef ENABLE_CHECKING
  int pid = getpid();
#endif

  va_start (va, format);


  message (LOG_EMERG, FACILITY_ALL, "Zfsd terminating due to internal error...\n");

  va_start (va, format);
  vsnprintf(msg, 1024, format, va);
  va_end (va);
  
  message (LOG_EMERG, FACILITY_ALL, msg);
  show_stackframe();
  sleep (2);

#ifdef ENABLE_CHECKING
  kill (pid, SIGABRT);
#endif

  /* Exit because in case of failure the state of data structures may be
     inconsistent.  */
  exit (EXIT_FAILURE);
}

/*! Report an "Aborted" internal error.  */
void
verbose_abort (const char *file, int line)
{
  internal_error ("Aborted by %lu, at %s:%d\n", (unsigned long) pthread_self (),
		  file, line);
}
