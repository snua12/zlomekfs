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

EXTRA_DIST = \
	zen-unit.spec \
	Doxyfile


SUBDIRS = \
	include \
	src 

rpm: dist
	./make-rpm.sh

doc: 
	doxygen

html: doc

install-doc: doc
	mkdir -p $(DESTDIR)/$(prefix)/share/doc/zen-unit
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(prefix)/share/doc/zen-unit
	mkdir -p $(DESTDIR)/$(prefix)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/zen-unit.h.3 -t $(DESTDIR)/$(prefix)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/ZEN_ASSERT.3 -t $(DESTDIR)/$(prefix)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/ZEN_TEST.3 -t$(DESTDIR)/$(prefix)/share/man/man3

install-html: install-doc

clean-local:
	rm -rf build