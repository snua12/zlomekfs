/*! \file win32api_test/file_tests.h
 *  \brief File tests
 *  \author Ales Snuparek
 */


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

#ifndef FILE_TESTS_H
#define FILE_TESTS_H
#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief tests some win32 api operations in selected directory
 *  \param path directory where is test performed
 */
void test_file_op(char * path);

/*! \brief cleanup after file_op test
 *  \param path directory where is test performed
 */
void cleanup_file_op(char * path);

/*! \brief creates the test file
 *  \param path name of the test file
 */
void create_test_file(char * path);

/*! \brief creates count of test files in a directory
 *  \param path direcotry where are test files created
 *  \param count how much of test files is created
 */
void generate_file_content(char * path, int count);

/*! \brief remove count of test files from a directory
 *  \param path direcotry where are test files created
 *  \param count how much of test files is created
 */
void cleanup_file_content(char * path, int count);

#ifdef __cplusplus
}
#endif

#endif

