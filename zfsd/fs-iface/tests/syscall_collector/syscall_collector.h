/*! \file \brief Directory tests*/

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#ifndef SYSCALL_COLLECTOR_H
#define SYSCALL_COLLECTOR_H
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum syscall_state_def
{
	SYSCALL_STATE_BEGIN,
	SYSCALL_STATE_END
} syscall_state;

typedef enum syscall_op_def
{
	SYSCALL_OP_OPEN = 0,
	SYSCALL_OP_CLOSE,
	SYSCALL_OP_WRITE,
	SYSCALL_OP_MKDIR,
	SYSCALL_OP_RMDIR,
	SYSCALL_OP_UNLINK,
	SYSCALL_OP_MAX	
} syscall_op;

void collector_init();
void collector_print();
void collect(syscall_op op, syscall_state state);


#ifdef __cplusplus
}
#endif

#endif

