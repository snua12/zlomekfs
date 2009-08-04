#ifndef  ZEN_LIBRARY_RUN_H 
#define  ZEN_LIBRARY_RUN_H 

/*! \file
    \brief 'Main' function for zen library (definition).

  The library 'main' function is implemented as library constructor and thus will be
  called by linker before binary real main. It will call exit preventing 
  real binary to run.

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

void LIB_CONSTRUCTOR init (void);

#endif /* ZEN_LIBRARY_RUN_H */