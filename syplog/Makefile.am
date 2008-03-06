# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of Syplog
#
# Syplog is free software; you can redistribute it and/or modify
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
# along with Syplog; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

EXTRA_DIST = \
	syplog.spec \
	Doxyfile \
	conf/etc/dbus-1/system.d/syplog.conf


SUBDIRS = \
	src \
	tests


rpm: dist
	./make-rpm.sh

test: all
	@$(MAKE) -C tests test
	@$(MAKE) -C src test

doc: 
	doxygen

html: doc

install-doc: doc
	mkdir -p $(DESTDIR)/$(prefix)/share/doc/syplog
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(prefix)/share/doc/syplog

install-exec-hook:
	mkdir -p $(DESTDIR)/etc/dbus-1/system.d
	$(INSTALL) -m 444 conf/etc/dbus-1/system.d/syplog.conf -t $(DESTDIR)/etc/dbus-1/system.d

install-html: install-doc

clean-local:
	rm -rf build
