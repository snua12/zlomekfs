# Makefile.
# Copyright (C) 2003 Josef Zlomek
#
# This file is part of ZFS.
#
# ZFS is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# ZFS is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License along with
# ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
# or download it from http://www.gnu.org/licenses/gpl.html

CC = gcc
LD = $(CC)
DEFS = 
CFLAGS = -g -Wall -pedantic
LDFLAGS = -lpthread
COMPILE = $(CC) $(DEFS) $(CFLAGS)
LINK = $(LD) $(CFLAGS) $(LDFLAGS) -o $@

.SUFFIXES: .c .o

.PHONY: all install uninstall clean distclean

PACKAGE = zfsd
OBJECTS = config.o dir.o file.o fh.o hashtab.o \
          log.o memory.o node.o volume.o zfsd.o

.c.o:
	$(COMPILE) -c $<

all: $(PACKAGE)

$(PACKAGE): $(OBJECTS)
	$(LINK) $(OBJECTS) $(LIBS)

zfs_prot.h: zfs_prot.x
# temporarily generate everything what can be generated
	rpcgen -a zfs_prot.x
  
clean:
	-rm -f $(PACKAGE) *.o core *.core
	-rm -f Makefile.zfs_prot zfs_prot.h zfs_prot_*

# Dependencies:
config.o: config.c config.h system.h log.h memory.h
dir.o: dir.c dir.h system.h fh.h hashtab.h
fh.o: fh.c fh.h system.h
file.o: file.c file.h system.h fh.h
hashtab.o: hashtab.c hashtab.h system.h log.h memory.h
log.o: log.c log.h system.h
memory.o: memory.c memory.h system.h log.h
node.o: node.c node.h system.h memory.h
volume.o: volume.c volume.h system.h memory.h
zfsd.o: zfsd.c zfsd.h system.h config.h log.h memory.h
