# Makefile.
# Copyright (C) 2003, 2004 Josef Zlomek
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
DEFS = -DENABLE_CHECKING -DTEST -DTEST_UPDATE
CFLAGS = -g -W -Wall -pedantic
LDFLAGS =
LIBS = -lpthread
COMPILE = $(CC) $(DEFS) $(CFLAGS)
LINK = $(LD) $(CFLAGS) $(LDFLAGS) -o $@

.SUFFIXES: .c .o

.PHONY: all install uninstall clean distclean

BINARIES = zfsd dump_intervals dump_metadata
ZFSD =	alloc-pool.o cap.o config.o constant.o crc32.o \
	data-coding.o dir.o fh.o fibheap.o file.o hashfile.o \
	hashtab.o interval.o kernel.o log.o md5.o memory.o \
	metadata.o network.o node.o queue.o random.o semaphore.o \
	splay-tree.o string-list.o test.o thread.o update.o \
	user-group.o util.o varray.o volume.o zfsd.o zfs_prot.o
DUMP_INTERVALS = alloc-pool.o dump_intervals.o interval.o log.o \
		 memory.o splay-tree.o util.o varray.o
DUMP_METADATA = alloc-pool.o constant.o crc32.o dump_metadata.o \
		fibheap.o hashfile.o hashtab.o interval.o log.o \
		memory.o metadata.o queue.o semaphore.o splay-tree.o \
		string-list.o thread.o util.o varray.o

.c.o:
	$(COMPILE) -c $<

all: $(BINARIES)

clean:
	-rm -f $(PACKAGE) *.o core *.core
distclean: clean
	-rm -f Makefile.dep

zfsd: $(ZFSD)
	$(LINK) $(ZFSD) $(LIBS)
dump_intervals: $(DUMP_INTERVALS)
	$(LINK) $(DUMP_INTERVALS) $(LIBS)
dump_metadata: $(DUMP_METADATA)
	$(LINK) $(DUMP_METADATA) $(LIBS)

include ./Makefile.dep
