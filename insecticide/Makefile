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

DIST_FILES = *.py Makefile insecticide.spec VERSION 

all: build

build:
	python setup.py build

install:
	python setup.py install --prefix "$(DESTDIR)/$(PREFIX)"

test:
	python -c 'import insecticide'

clean: 
	python setup.py clean
	rm -rf insecticide-*.tar.gz build dist *.pyc build insecticide.egg-info

package: clean
	rm -rf build/package
	mkdir -p build/package/insecticide-$(VERSION)
	cp -rf ${DIST_FILES} build/package/insecticide-$(VERSION)
	#TODO: exclude .svn
	tar -C build/package -czf insecticide-$(VERSION).tar.gz insecticide-$(VERSION)

rpm: package
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	rpmbuild -ta insecticide-$(VERSION).tar.gz --clean --define _topdir`pwd`/build
	
