/*! \file POSIXapi_test/dir_tests.h
 *  \brief Directory tests
 *  \author Ales Snuparek
 *
 * This file contains some directory tests
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

#ifndef DIR_TESTS_H
#define DIR_TESTS_H
#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief create tree with directories */
void generate_directory_content(char * path, int count, int deep);

/*! \brief cleanup after directory tree test */
void cleanup_directory_content(char * path, int count, int deep);

#ifdef __cplusplus
}
#endif

#endif

