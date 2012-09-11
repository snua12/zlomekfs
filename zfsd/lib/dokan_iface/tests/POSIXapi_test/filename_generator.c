/* ! \file \brief Random walk generator*/

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


#include <stdlib.h>
#include "filename_generator.h"

void init_filename_generator(void)
{
	// initialize random generator to fixed seed
	srandom(1);
}

void get_filename(char name[])
{
	int i;
	long int rnd = random();
	for (i = 0; i < 4; ++i)
	{
		int index = rnd % 32;
		rnd /= 32;
		if (index < 26)
			name[i] = index + 'a';
		else
			name[i] = index + '0';
	}

	name[4] = 0;
}


