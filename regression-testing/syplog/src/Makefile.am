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
# Syplog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with syplog; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/control \
	-I$(top_srcdir)/src/media \
	-I$(top_srcdir)/src/formatters \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/dbus-1.0/include


AM_CFLAGS = -DENABLE_CHECKING

lib_LTLIBRARIES = libsyplog.la

nobase_pkginclude_HEADERS = \
	facility.h \
	log-constants.h \
	log-level.h \
	log-struct.h \
	settings-loader.h \
	syp-error.h \
	syplog.h \
	control/control.h \
	control/control-protocol.h \
	control/listener.h \
	media/file-medium.h \
	media/print-medium.h \
	media/medium-api.h \
	media/medium.h \
	media/shm-medium.h \
	formatters/formatter-api.h \
	formatters/formatter.h \
	formatters/raw-formatter.h \
	formatters/user-readable-formatter.h


libsyplog_la_SOURCES = \
	syplog.c \
	formatters/formatter.c \
	settings-loader.c \
	formatters/user-readable-formatter.c \
	formatters/raw-formatter.c \
	media/medium.c \
	media/file-medium.c \
	media/print-medium.c \
	media/shm-medium.c \
	media/medium-api.c \
	syp-error.c \
	control/control-protocol.c \
	control/listener.c \
	control/control.c

libsyplog_la_CFLAGS = -pthread -fPIC
libsyplog_la_LIBADD = -lpthread -ldbus-1

test: all
	./test.py ./.libs/libsyplog.so.0.0.0
