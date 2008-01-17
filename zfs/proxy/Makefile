# Makefile.
# Copyright (C) 2003, 2004 Josef Zlomek
# Copyright (C) 2006 Miloslav Trmac <mitr@volny.cz>
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
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wundef \
	-Wwrite-strings
LD = gcc
LD_OPTS = $(FUSE_LDFLAGS) -lpthread

#FUSE_CFLAGS := $(shell pkg-config --cflags fuse)
FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -I../fuse/include
#FUSE_LDFLAGS := $(shell pkg-config --libs fuse)
FUSE_LDFLAGS= -pthread ../fuse/lib/.libs/libfuse.a -lrt -ldl

ifeq ($(debug), 1)
  CC_OPTS += -g -DDEBUG -O0
  LD_OPTS += -g
else
  CC_OPTS += -O2 -g
endif


.SUFFIXES: .c .o
.PHONY: all install uninstall clean

BB_ALL = zfs-proxy

all: $(BB_ALL)


# Executables

OO_ZFS_PROXY = proxy.o \
	       crc32.o \
	       data-coding.o \
	       hashtab.o \
	       log.o \
	       memory.o \
	       util.o \
	       varray.o \
	       zfs-prot.o

zfs-proxy: $(OO_ZFS_PROXY)
	$(LD) -o $@ $^ $(LD_OPTS)

# Dependencies

OO_ALL = $(sort $(OO_ZFS_PROXY))

include $(OO_ALL:.o=.dep)

%.dep: %.c
	set -e ; $(CC) -MM $(CC_OPTS) $< | sed 's/\($*\)\.o *:/\1.o $@:/g' > $@

# Objects

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

# Other

clean:
	-rm -f $(BB_ALL) $(OO_ALL) $(OO_ALL:.o=.dep)

install:
	$(INSTALL) -m 755 zfs-proxy $(PREFIX)/sbin

uninstall:
	-rm -f $(PREFIX)/sbin/zfs-proxy

