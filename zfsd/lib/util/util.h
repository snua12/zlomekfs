/* ! \file \brief Helper functions.  */

/* Copyright (C) 2003 Josef Zlomek

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

#ifndef UTIL_H
#define UTIL_H

#include "system.h"
#include <stdio.h>
#include <stddef.h>

extern void print_hex_buffer(int level, FILE * f, char *buf, unsigned int len);
extern bool full_read(int fd, void *buf, size_t len);
extern bool full_write(int fd, void *buf, size_t len);
extern bool full_mkdir(char *path, unsigned int mode);
extern bool bytecmp(const void *p, int byte, size_t len);

#endif
