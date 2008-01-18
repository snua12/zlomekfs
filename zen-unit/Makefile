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
VERSION ?= `cat VERSION`

INSTALL = install -o root -g root 

all: lib doc

doc: doc/html doc/man

doc/man: Doxyfile 
	doxygen Doxyfile

doc/html: Doxyfile
	doxygen Doxyfile

lib: 
	@$(MAKE) -C src

test:
	@$(MAKE) -C tests test

clean:
	rm -rf doc/*
	@$(MAKE) -C src clean
	@$(MAKE) -C tests clean
	rm -f doxy-warnings
	rm -f zen-unit-*.tar.gz

install: lib include/zen-unit.h
	mkdir -p $(DESTDIR)/$(PREFIX)/lib
	mkdir -p $(DESTDIR)/$(PREFIX)/include
	$(INSTALL) -m 755 src/libzen-unit.so -t $(DESTDIR)/$(PREFIX)/lib
	$(INSTALL) -m 444 include/zen-unit.h -t $(DESTDIR)/$(PREFIX)/include

uninstall: 
	-rm -f $(DESTDIR)/$(PREFIX)/lib/libzen-unit.so
	-rm -f $(DESTDIR)/$(PREFIX)/include/zen-unit.h


install-doc: doc
	mkdir -p $(DESTDIR)/$(PREFIX)/share/doc/zen-unit
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(PREFIX)/share/doc/zen-unit
	mkdir -p $(DESTDIR)/$(PREFIX)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/zen-unit.h.3 -t $(DESTDIR)/$(PREFIX)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/ZEN_ASSERT.3 -t $(DESTDIR)/$(PREFIX)/share/man/man3
	$(INSTALL) -m 444 doc/man/man3/ZEN_TEST.3 -t$(DESTDIR)/$(PREFIX)/share/man/man3

uninstall-doc:
	rm -rf $(DESTDIR)/$(PREFIX)/share/doc/zen-unit
	rm -rf $(DESTDIR)/$(PREFIX)/share/man/man3/zen-unit.h.3
	rm -rf $(DESTDIR)/$(PREFIX)/share/man/man3/ZEN_ASSERT.3
	rm -rf $(DESTDIR)/$(PREFIX)/share/man/man3/ZEN_TEST.3



package: clean
	rm -rf /tmp/zen-unit-build
	mkdir -p /tmp/zen-unit-build/zen-unit-${VERSION}
	cp -rf ./* /tmp/zen-unit-build/zen-unit-${VERSION}
	tar -C /tmp/zen-unit-build -czf zen-unit-${VERSION}.tar.gz zen-unit-${VERSION}
	rm -rf /tmp/zen-unit-build

rpm: package
	rpmbuild -ta zen-unit-${VERSION}.tar.gz
