# Makefile.am
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

noinst_PROGRAMS = zentests_fail zentests_pass

zentests_pass_SOURCES = zen-unit-pass.c
zentests_fail_SOURCES = zen-unit-fail.c




test: zentests_pass zentests_fail
	LD_PRELOAD=libzenunit.so LD_LIBRARY_PATH=.:../src/.libs ./zentests_pass
	if LD_PRELOAD=libzenunit.so LD_LIBRARY_PATH=.:../src/.libs ./zentests_fail; then false; else true; fi

