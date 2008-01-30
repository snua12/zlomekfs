# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of TestResultStorage
#
# TestResultStorage is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# TestResultStorage is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with TestResultStorage; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

all: build

PREFIX ?= /usr/local
VERSION ?= `cat VERSION`

PACKAGE_FILES = Makefile setup.py src VERSION TestResultStorage.spec

build:
	./setup.py build

test:
	python -c 'import TestResultStorage'
	python -c 'import TestResultStorage.resultRepository'

install:
	./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"

clean:
	rm -rf MANIFEST build dist src/*.pyc src/resultRepository/*.pyc TestResultStorage-*.tar.gz *.egg-info build

package: clean
	rm -rf build/package
	mkdir -p build/package/TestResultStorage-${VERSION}
	cp -rf ${PACKAGE_FILES} build/package/TestResultStorage-${VERSION}
	tar -C build/package -czf TestResultStorage-${VERSION}.tar.gz TestResultStorage-${VERSION}
	rm -rf build/package

rpm: package
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	rpmbuild -ta TestResultStorage-${VERSION}.tar.gz --clean --define _topdir`pwd`/build
