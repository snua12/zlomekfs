/*! \file syscall_collector/syscall_collector.h
 *  \brief Collect statistic for some file syscalls
 *  \author Ales Snuparek
 *
 *
 * This test creates a "searching in depth" tree of directories that on
 * the leaf level include files. Then remove asresarovou structure.
 * For the following operations: open, read, write, close mkdir and
 * rmdir is measured by the mean duration of these operations.
 * This test uses posix API.
 */

/* Copyright (C) 2008, 2012 Ales Snuparek

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

/*! \brief specifies if syscall is start or syscall is terminated */
typedef enum syscall_state_def
{
	SYSCALL_STATE_BEGIN,
	SYSCALL_STATE_END
} syscall_state;

/*! \brief represents each syscall */
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

/*! \brief initializes syscall collector internal structures */
void collector_init();

/*! \brief print syscall collector results */
void collector_print();

/*! \brief call before and after syscall inorder to collect syscall time consumption */
void collect(syscall_op op, syscall_state state);


#ifdef __cplusplus
}
#endif

#endif

