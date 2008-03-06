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

INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = libzenunit.la
bin_PROGRAMS = zenunit

noinst_HEADERS = \
	zen-defs.h \
	zen-elf-search.h \
	zen-error.h \
	zen-search.h \
	zen-shared-search.h \
	zen-static-search.h 

libzenunit_la_SOURCES = \
	zen-search.c \
	zen-static-search.c \
	zen-shared-search.c \
	zen-elf-search.c \
	zen-library-run.c

libzenunit_la_CFLAGS = -pthread -fPIC
libzenunit_la_LDFLAGS = --no-undefined -shared
libzenunit_la_LIBADD = -lelf -lpthread

zenunit_SOURCES = zen-run.c
zenunit_LDADD = ${top_builddir}/src/libzenunit.la

