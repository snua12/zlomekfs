#ifndef PRINT_MEDIUM_H
#define PRINT_MEDIUM_H

/* ! \file \brief Print medium functions definitions.  */

/* Copyright (C) 2010 Rastislav Wartiak

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


#define _GNU_SOURCE
#include <stdio.h>
#undef _GNU_SOURCE

#include "log-constants.h"
#include "medium-api.h"

/// name of medium for translation from options (--medium=print)
#define	PRINT_MEDIUM_NAME	"print"

/* ! Structure that holds internal state info specific for print medium. */
typedef struct print_medium_def
{
	/// handler of stdin or stdout
	FILE *handler;
} *print_medium;

/* ! Parse params and initialize print medium If NULL argv is given, defaults
   will be used.

   @see open_medium @see medium-api.h */
syp_error open_print_medium(struct medium_def *target, int argc,
							const char **argv);

/* ! Close print medium and free print medium specific structures @see
   close_medium @see medium-api.h */
syp_error close_print_medium(struct medium_def *target);

/* ! do operation on log @see access_medium @see medium-api.h */
syp_error print_access(struct medium_def *target, log_struct log);

/* ! prints print options help to fd. @see print_medium_help @see medium-api.h */
void print_print_medium_help(int fd, int tabs);

#endif /* PRINT_MEDIUM_H */
