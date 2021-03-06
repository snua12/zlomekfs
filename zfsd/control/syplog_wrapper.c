/**
 *  \file syplog_wrapper.c
 * 
 *  \author Ales Snuparek
 *
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


#include "syplog_wrapper.h"
#include "syplog.h"

syp_error control_wrap_set_log_level(logger glogger, log_level_t level)
{
	return set_log_level(glogger, level);
}

syp_error control_wrap_set_facility(logger glogger, facility_t facility)
{
	return set_facility(glogger, facility);
}

syp_error control_wrap_reset_facility(logger glogger, facility_t facility)
{
	return reset_facility(glogger, facility);
}
