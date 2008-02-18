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


PREFIX ?= /usr

VERSION ?= $(shell cat VERSION)

RELEASE ?= $(shell if [ -f "RELEASE" ]; then cat RELEASE; else echo 0; fi)

ifeq ($(REVISION),)
        REVISION:= $(shell svnversion | sed -e 's/\([0-9]*\).*/\1/')
endif
ifeq ($(REVISION),)
	REVISION = 0
endif

PACKAGE_FILES = src Doxyfile conf doc tests TODO VERSION syplog.spec Makefile


INSTALL = install
# -o root -g root 

all: lib py doc

doc: doc/html

doc/html: Doxyfile
	doxygen Doxyfile

lib: 
	@$(MAKE) -C src

py:
	@$(MAKE) -C py

test:
	@$(MAKE) -C tests test

clean:
	@$(MAKE) -C src clean
	@$(MAKE) -C tests clean
	@$(MAKE) -C py clean
	rm -rf doc/* build
	rm -f doxy-warnings
	rm -f syplog-*.tar.gz

install: 
	@$(MAKE) -C src install
	mkdir -p $(DESTDIR)/etc/dbus-1/system.d
	$(INSTALL) -m 444 conf/etc/dbus-1/system.d/syplog.conf -t $(DESTDIR)/etc/dbus-1/system.d

uninstall: 
	@$(MAKE) -C src uninstall
	rm -f $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d/syplog.conf



install-doc: doc
	mkdir -p $(DESTDIR)/$(PREFIX)/share/doc/syplog
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(PREFIX)/share/doc/syplog

uninstall-doc:
	rm -rf $(DESTDIR)/$(PREFIX)/share/doc/syplog

package: clean
	rm -rf build/package
	mkdir -p build/package/syplog-$(VERSION).$(REVISION)
	cp ${PACKAGE_FILES} build/package/syplog-$(VERSION).$(REVISION) -rf
	#todo: remove .svn files
	tar -C build/package -czf syplog-$(VERSION).$(REVISION).tar.gz syplog-$(VERSION).$(REVISION)

rpm: package
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	expr ${RELEASE} + 1 > RELEASE
	rpmbuild -ta syplog-$(VERSION).$(REVISION).tar.gz --clean --define "_topdir `pwd`/build" --define "REVISION ${REVISION}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}
	
