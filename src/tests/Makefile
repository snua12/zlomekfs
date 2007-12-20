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
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wundef -pthread -I`pwd`/../syplog -I`pwd`/../syplog/formaters -I`pwd`/../syplog/media -I`pwd`/../syplog/control
#	-Wwrite-strings
LD = gcc

LD_OPTS += -lpthread -L`pwd`/../syplog -lsyplog


ifeq ($(debug), 1)
  CC_OPTS += -g -DDEBUG -DENABLE_CHECKING -O0 -ggdb -g3
  LD_OPTS += -g
else
  CC_OPTS += -O2 -g
endif

.SUFFIXES: .c .o
.PHONY: all install uninstall clean

BB_ALL = dump-logs fakelog log-control

all: $(BB_ALL)


# Executables

OO_DUMP-LOGS = dump-logs.o

OO_LOG-CONTROL = log-control.o \
	../syplog/control/control.o \
	../syplog/syp-error.c

OO_FAKELOG = fakelog.o

dump-logs: $(OO_DUMP-LOGS)
	$(LD) -o $@ $^ $(LD_OPTS)  

log-control: $(OO_LOG-CONTROL)
	$(LD) -o $@ $^ $(LD_OPTS)  

fakelog: $(OO_FAKELOG)
	$(LD) -o $@ $^ $(LD_OPTS)  

# Dependencies

OO_ALL = $(sort $(OO_DUMP-LOGS)) $(sort $(OO_FAKELOG))

include $(OO_ALL:.o=.dep)

%.dep: %.c
	set -e ; $(CC) -MM $(CC_OPTS) $< | sed 's/\($*\)\.o *:/\1.o $@:/g' > $@

# Objects

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

# Other

clean:
	-rm -f $(BB_ALL) $(OO_ALL) $(OO_ALL:.o=.dep)

dry-test: dump-logs local.log
	./dump-logs --reader=file --log-file=local.log --formater=raw