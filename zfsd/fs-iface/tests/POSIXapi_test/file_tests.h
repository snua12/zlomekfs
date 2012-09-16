/* ! \file \brief File tests*/

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

#ifndef FILE_TESTS_H
#define FILE_TESTS_H
#ifdef __cplusplus
extern "C"
{
#endif

void test_file_op(char * path);

void cleanup_file_op(char * path);

void create_test_file(char * path);

void generate_file_content(char * path, int count);

void cleanup_file_content(char * path, int count);

#ifdef __cplusplus
}
#endif

#endif

