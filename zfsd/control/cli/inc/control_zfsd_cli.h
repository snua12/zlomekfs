/**
 *  \file control_zfsd_cli.h 
 * 
 *  \author Ales Snuparek
 *  \brief CLI control interface inplementation
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

#ifndef CONTROL_ZFSD_CLI_H
#define CONTROL_ZFSD_CLI_H

#ifdef __cplusplus
extern "C"
{
#endif

//! starts CLI control interface
void start_cli_control(void);

//! stops CLI control interface
void stop_cli_control(void);

#ifdef __cplusplus
}
#endif

/*! \page zfs-cli ZlomekFS CLI interface
   ZlomekFS CLI is based on http://alexis.royer.free.fr/CLI.

   Generated user documentation is below:
    \htmlinclude zfs_cli.html
*/

#endif
