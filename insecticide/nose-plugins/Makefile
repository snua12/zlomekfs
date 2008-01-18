# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of insecticide
#
# insecticide is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# insecticide is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with insecticide; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

PREFIX ?= "/usr/local"

VERSION ?= `cat VERSION`

all: build

build:
	python setup.py build

install:
	python setup.py install --prefix "$(DESTDIR)/$(PREFIX)"

clean: 
	python setup.py clean
	rm -rf insecticide-*.tar.gz

package: clean
	rm -rf /tmp/insecticide-build
	mkdir -p /tmp/insecticide-build/insecticide-$(VERSION)
	cp -rf ./* /tmp/insecticide-build/insecticide-$(VERSION)
	tar -C /tmp/insecticide-build -czf insecticide-$(VERSION).tar.gz insecticide-$(VERSION)

rpm: package
	rpmbuild -ta insecticide-$(VERSION).tar.gz
	
