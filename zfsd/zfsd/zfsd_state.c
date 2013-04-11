/**
 *  \file zfsd_state.c
 * 
 *  \brief This file keeps zlomekFS state (eg. ZFSD_STATE_STARTING, ZFSD_STATE_RUNNING ..)
 *  \author Ales Snuparek 
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

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


#include "system.h"
#include "zfsd_state.h"

static zfsd_state_e zfsd_state = ZFSD_STATE_STARTING;

void zfsd_set_state(zfsd_state_e state)
{
	zfsd_state = state;
}

zfsd_state_e zfsd_get_state(void)
{
	return zfsd_state;
}
