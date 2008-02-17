# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of Zen-unit
#
# Zen-unit is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zen-unit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Zen-unit; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

PREFIX ?= /usr

INSTALL = install

PROJECT_ROOT=`pwd`/..

CC = gcc
CC_OPTS = -Wall -W -Wcast-align -Wcast-qual -Wmissing-noreturn \
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wundef \
	-I${PROJECT_ROOT}/includev -fPIC

LD = gcc


ifeq ($(debug), 1)
  CC_OPTS += -g -DDEBUG -O0 -ggdb -g3
  LD_OPTS += -g
else
  CC_OPTS += -O2 -g
endif

.SUFFIXES: .c .o
.PHONY: all install uninstall clean

all: zen-tests

# Executables

libzentests.so: zen-unit-so.o
	gcc  -shared -o libzentests.so zen-unit-so.o $(LD_OPTS)

zen-tests: zen-unit-binary.o libzentests.so
	gcc -o zen-tests -L. -lzentests zen-unit-binary.o $(LD_OPTS)

# Objects

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

# Other

clean:
	-rm -f *.so *.o zen-tests

test: libzentests.so zen-tests
	LD_PRELOAD=libzen-unit.so LD_LIBRARY_PATH=.:$(PROJECT_ROOT)/src ./zen-tests
