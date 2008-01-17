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
INSTALL = install -o root -g root 

all: doc lib py

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
	rm -rf doc/*
	rm -f doxy-warnings

install: 
	@$(MAKE) -C src install
	@$(MAKE) -C py install
	mkdir -p $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d
	@$(INSTALL) -m 444 conf/etc/dbus-1/system.d/syplog.conf -t $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d

uninstall: 
	@$(MAKE) -C src uninstall
	@$(MAKE) -C py uninstall
	rm -f $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d/syplog.conf



install-doc: doc
	mkdir -p $(DESTDIR)/$(PREFIX)/share/doc/syplog
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(PREFIX)/share/doc/syplog

uninstall-doc:
	rm -rf $(DESTDIR)/$(PREFIX)/share/doc/syplog
