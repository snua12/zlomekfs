/*! \file
    \brief Main function for zen-unit binary.
    @see zen-library-run.h

  This function is just fake, won't be called.
  The real work is done by loaded shared library which 
  constructor will execute the tests and exit before this
  main can be executed.
*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Zen Unit.

   Zen-unit is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Zen-unit is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Zen-unit; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#include "zen-defs.h"
#include "zen-search.h"

int main (int * argc, char ** argv)
{
	// try to segfault to make BIG alarm that something is wrong
	memset (0, 0, 10000);
	exit (1);
}

