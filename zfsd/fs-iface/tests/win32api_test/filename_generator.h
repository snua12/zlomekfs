/*! \file win32api_test/filename_generator.h
 *  \brief Random filename generator
 *  \author Ales Snuparek
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


#ifndef FILENAME_GENERATOR_H
#define FILENAME_GENERATOR_H
#ifdef __cplusplus
extern "C"
{
#endif
	/// initialize generator
	void init_filename_generator(void);

	/// get filename from fixed sequence
	void get_filename(char name[]);

#ifdef __cplusplus
}
#endif

#endif

