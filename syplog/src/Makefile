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


all: libsyplog.a libsyplog.so

CFLAGS += -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -I.
CSFLAGS = $(CFLAGS) -fPIC
INSTALL = install 
#-o root -g root 

PREFIX ?= /usr

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.os: %.c
	$(CC) $(CSFLAGS) -c -o $@ $<

OO_SYPLOG = syplog.o \
	formatters/formatter.o \
	settings-loader.o \
	formatters/user-readable-formatter.o \
	formatters/raw-formatter.o \
	media/medium.o \
	media/file-medium.o \
	media/shm-medium.o \
	media/medium-api.o \
	syp-error.o \
	control/control-protocol.o \
	control/listener.o \
	control/control.o

OO_SYPLOG_SO = syplog.os \
	formatters/formatter.os \
	settings-loader.os \
	formatters/user-readable-formatter.os \
	formatters/raw-formatter.os \
	media/medium.os \
	media/file-medium.os \
	media/shm-medium.os \
	media/medium-api.os \
	syp-error.os \
	control/control-protocol.os \
	control/listener.os \
	control/control.os


libsyplog.a: $(OO_SYPLOG)
	ar -cru -o $@ $^
libsyplog.so: $(OO_SYPLOG_SO)
	gcc -shared -o $@ $^



install: libsyplog.a libsyplog.so
	mkdir -p $(DESTDIR)/$(PREFIX)/lib
	mkdir -p $(DESTDIR)/$(PREFIX)/include/syplog
	mkdir -p $(DESTDIR)/$(PREFIX)/include/syplog/formatters
	mkdir -p $(DESTDIR)/$(PREFIX)/include/syplog/control
	mkdir -p $(DESTDIR)/$(PREFIX)/include/syplog/media
	$(INSTALL) -m 755 libsyplog.a libsyplog.so -t $(DESTDIR)/$(PREFIX)/lib
	$(INSTALL) -m 444  *.h -t $(DESTDIR)/$(PREFIX)/include/syplog
	$(INSTALL) -m 444  formatters/*.h -t $(DESTDIR)/$(PREFIX)/include/syplog/formatters
	$(INSTALL) -m 444  control/*.h -t $(DESTDIR)/$(PREFIX)/include/syplog/control
	$(INSTALL) -m 444  media/*.h -t $(DESTDIR)/$(PREFIX)/include/syplog/media

uninstall:
	-rm -f $(DESTDIR)/$(PREFIX)/lib/libsyplog.a
	-rm -f $(DESTDIR)/$(PREFIX)/lib/libsyplog.so
	-rm -rf $(DESTDIR)/$(PREFIX)/include/syplog

clean:
	rm -f *.o *.os */*.o */*.os *.a *.so
