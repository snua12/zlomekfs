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

CC = g++
LD = $(CC)
DEFS = 
CFLAGS = -g -Wall -pedantic
LDFLAGS =
LIBS = -lpthread
COMPILE = $(CC) $(DEFS) $(CFLAGS)
LINK = $(LD) $(CFLAGS) $(LDFLAGS) -o $@

.SUFFIXES: .c .o

.PHONY: all install uninstall clean distclean

PACKAGE = zfsd
OBJECTS = alloc-pool.o client.o config.o crc32.o dir.o file.o fh.o hashtab.o \
          interval.o log.o memory.o node.o queue.o \
          server.o splay-tree.o thread.o volume.o zfsd.o
RPC_OBJECTS = zfs_prot_svc.o zfs_prot_server.o zfs_prot_xdr.o

.c.o:
	$(COMPILE) -c $<

all: $(PACKAGE)

clean:
	-rm -f $(PACKAGE) *.o core *.core
	-rm -f Makefile.zfs_prot zfs_prot.h zfs_prot_*

$(PACKAGE): $(OBJECTS) $(RPC_OBJECTS)
	$(LINK) $(OBJECTS) $(RPC_OBJECTS) $(LIBS)

# RPC code
zfs_prot.h: zfs_prot.x
	rpcgen -h zfs_prot.x > zfs_prot.h
zfs_prot_svc.c: zfs_prot.x zfs_prot.h
	rpcgen -m zfs_prot.x > $@
zfs_prot_server.c: zfs_prot.x zfs_prot.h
	rpcgen -Ss zfs_prot.x > $@
zfs_prot_xdr.c: zfs_prot.x zfs_prot.h
	rpcgen -c zfs_prot.x > $@
zfs_prot_svc.o: zfs_prot_svc.c
zfs_prot_server.o: zfs_prot_server.c
zfs_prot_xdr.o: zfs_prot_xdr.c
  
# Dependencies:
alloc-pool.o: alloc-pool.c alloc-pool.h system.h log.h memory.h
client.o: client.c client.h system.h log.h memory.h thread.h
config.o: config.c config.h system.h log.h memory.h
crc32.o: crc32.c crc32.h system.h
dir.o: dir.c dir.h system.h fh.h hashtab.h
fh.o: fh.c fh.h system.h
file.o: file.c file.h system.h fh.h
hashtab.o: hashtab.c hashtab.h system.h log.h memory.h
interval.o: interval.c interval.h system.h log.h memory.h splay-tree.h
log.o: log.c log.h system.h
memory.o: memory.c memory.h system.h log.h
node.o: node.c node.h system.h memory.h
queue.o: queue.c queue.h system.h log.h memory.h
server.o: server.c server.h system.h log.h thread.h zfs_prot.h
splay-tree.o: splay-tree.c splay-tree.h system.h alloc-pool.h log.h memory.h
thread.o: thread.c thread.h system.h log.h memory.h
volume.o: volume.c volume.h system.h memory.h
zfsd.o: zfsd.c zfsd.h system.h client.h config.h log.h memory.h server.h
