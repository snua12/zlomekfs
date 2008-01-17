# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of syplog
#
# syplog is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# syplog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with syplog; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

CC = gcc

PROJECT_ROOT = `pwd`/..

CFLAGS += -Wall -W -Wcast-align -Wcast-qual -Wmissing-noreturn \
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wundef  \
	-pthread -I${PROJECT_ROOT}/src \
	-I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include


LD = gcc

LD_OPTS += -lpthread -L$(PROJECT_ROOT)/src -lsyplog -ldbus-1



all: dump-logs fakelog log-control

LIB = ${PROJECT_ROOT}/src/libsyplog.a

$(LIB):
	make -C ${PROJECT_ROOT}/src


# Executables


dump-logs: dump-logs.o ${LIB}
	$(LD) -o $@ $^ $(LD_OPTS)

log-control: log-control.o ${LIB}
	$(LD) -o $@ $^ $(LD_OPTS)

fakelog: fakelog.o ${LIB}
	$(LD) -o $@ $^ $(LD_OPTS)  

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o dump-logs log-control fakelog

test: dump-logs fakelog log-control
	true
