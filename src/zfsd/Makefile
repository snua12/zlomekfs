# Makefile.
# Copyright (C) 2003, 2004 Josef Zlomek
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of zlomekFS.
#
# zlomekFS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zlomekFS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with zlomekFS; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

PREFIX ?= /usr

INSTALL = install

CC = gcc
CC_OPTS = $(FUSE_CFLAGS) -Wall -W -Wcast-align -Wcast-qual -Wmissing-noreturn \
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wundef -pthread
#	-Wwrite-strings
LD = gcc

#FUSE_CFLAGS := $(shell pkg-config --cflags fuse)
FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -I../fuse/include
#FUSE_LDFLAGS := $(shell pkg-config --libs fuse)
FUSE_LDFLAGS= -pthread ../fuse/lib/.libs/libfuse.a
ifndef freebsd
  FUSE_LDFLAGS += -lrt -ldl
endif

ifeq ($(syslog),1)
  LD_OPTS += $(FUSE_LDFLAGS) -lpthread
  CC_OPTS += -DUSE_SYSLOG
else
  LD_OPTS += $(FUSE_LDFLAGS) -lpthread
endif


ifeq ($(debug), 1)
  CC_OPTS += -g -DDEBUG -DENABLE_CHECKING -O0 -ggdb -g3
  LD_OPTS += -g
else
  CC_OPTS += -O2 -g
endif

.SUFFIXES: .c .o
.PHONY: all install uninstall clean

BB_ALL = zfsd dump-metadata dump-intervals

all: $(BB_ALL)


# Executables

OO_ZFSD = zfsd.o \
	  alloc-pool.o \
	  cap.o config.o constant.o crc32.o \
	  data-coding.o dir.o \
	  fh.o fibheap.o file.o \
	  hardlink-list.o hashfile.o hashtab.o \
	  interval.o \
	  journal.o \
	  kernel.o \
	  log.o \
	  md5.o memory.o metadata.o \
	  network.o node.o \
	  pthread.o \
	  queue.o \
	  random.o \
	  semaphore.o splay-tree.o \
	  thread.o \
	  update.o user-group.o util.o \
	  varray.o volume.o\
	  zfs-prot.o

zfsd: $(OO_ZFSD)
	$(LD) -o $@ $^ $(LD_OPTS)

OO_DUMP_METADATA = dump-metadata.o \
		   alloc-pool.o \
		   constant.o crc32.o \
		   fibheap.o \
		   hardlink-list.o hashfile.o hashtab.o \
		   interval.o \
		   journal.o \
		   log.o \
		   memory.o metadata.o \
		   node.o \
		   pthread.o \
		   splay-tree.o \
		   user-group.o \
		   util.o \
		   varray.o

dump-metadata: $(OO_DUMP_METADATA)
	$(LD) $(LD_OPTS) -o $@ $^

OO_DUMP_INTERVALS = dump-intervals.o \
		    alloc-pool.o \
		    interval.o \
		    log.o \
		    memory.o \
		    splay-tree.o \
		    util.o \
		    varray.o

dump-intervals: $(OO_DUMP_INTERVALS)
	$(LD) $(LD_OPTS) -o $@ $^


# Dependencies

OO_ALL = $(sort $(OO_ZFSD) $(OO_DUMP_METADATA) $(OO_DUMP_INTERVALS))

include $(OO_ALL:.o=.dep)

%.dep: %.c
	set -e ; $(CC) -MM $(CC_OPTS) $< | sed 's/\($*\)\.o *:/\1.o $@:/g' > $@

# Objects

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

# Other

clean:
	-rm -f $(BB_ALL) $(OO_ALL) $(OO_ALL:.o=.dep)

install: zfsd dump-intervals dump-metadata
	$(INSTALL) -m 755 zfsd $(PREFIX)/sbin
	$(INSTALL) -m 755 dump-intervals dump-metadata $(PREFIX)/bin

uninstall:
	-rm -f $(PREFIX)/sbin/zfsd
	-rm -f $(PREFIX)/bin/dump-intervals $(PREFIX)/bin/dump-metadata

