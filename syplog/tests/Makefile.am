# Makefile.am
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

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/control \
	-I$(top_srcdir)/src/media \
	-I$(top_srcdir)/src/formatters \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/dbus-1.0/include

AM_CFLAGS = -pthread

noinst_PROGRAMS = dumpLogs logControl fakelog infinite-log

dumpLogs_SOURCES = dump-logs.c
dumpLogs_LDADD = ${top_builddir}/src/libsyplog.la -lpthread -ldbus-1

logControl_SOURCES = log-control.c
logControl_LDADD = ${top_builddir}/src/libsyplog.la -lpthread -ldbus-1

fakelog_SOURCES = fakelog.c
fakelog_LDADD = ${top_builddir}/src/libsyplog.la -lpthread -ldbus-1

infinite_log_SOURCES = infinite-log.c
infinite_log_LDADD = ${top_builddir}/src/libsyplog.la -lpthread -ldbus-1

test: dumpLogs fakelog logControl
	LD_PRELOAD=libzenunit.so ./fakelog

clean-local:
	rm -rf biglog
