/* Logging functions.
   Copyright (C) 2003 Josef Zlomek

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

#ifndef LOG_H
#define LOG_H

#include "system.h"
#include <stdio.h>
#include <stdlib.h>

#define abort() verbose_abort(__FILE__, __LINE__)

/* Level of verbosity.  Higher number means more messages.  */
extern int verbose;

/* Print message to F if LEVEL > VERBOSE.  */
extern void message (int level, FILE *f, char *format, ...) ATTRIBUTE_PRINTF_3;

/* Report an internal error.  */
extern void internal_error (char *format, ...) ATTRIBUTE_NORETURN;

/* Report an "Aborted" internal error.  */
extern void verbose_abort (const char *file, int line) ATTRIBUTE_NORETURN;

#endif
