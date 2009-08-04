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

VERSION ?= $(shell cat VERSION)

RELEASE ?= $(shell if [ -f "RELEASE" ]; then cat RELEASE; else echo 0; fi)

TEMPLATES = \
	templates/resultRepository/default_page.html \
	templates/resultRepository/batchrun_detail.html \
	templates/resultRepository/batchrun_list.html \
	templates/resultRepository/testrun_detail.html \
	templates/resultRepository/testrun_list.html \
	templates/resultRepository/project_list.html

MEDIA = webMedia/style.css
PACKAGE_FILES = Makefile \
	setup.py \
	src \
	VERSION \
	TestResultStorage.spec \
	templates \
	webMedia


ifeq ($(REVISION),)
        REVISION:= $(shell svnversion | sed -e 's/\([0-9]*\).*/\1/')
endif
ifeq ($(REVISION),)
	REVISION = 0
endif

build:
	./setup.py build

test:
	python -c 'import TestResultStorage'
	python -c 'import TestResultStorage.resultRepository'

install:
	./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"

install-data:
	mkdir -p $(DESTDIR)/var/lib/TestResultStorage/templates/resultRepository
	mkdir -p $(DESTDIR)/var/lib/TestResultStorage/data
	mkdir -p $(DESTDIR)/var/lib/TestResultStorage/webMedia
	cp $(TEMPLATES) $(DESTDIR)/var/lib/TestResultStorage/templates/resultRepository
	cp $(MEDIA) $(DESTDIR)/var/lib/TestResultStorage/webMedia
	

clean:
	rm -rf MANIFEST build dist src/*.pyc src/resultRepository/*.pyc TestResultStorage-*.tar.gz *.egg-info build

dist: clean
	rm -rf build/package
	mkdir -p build/package/TestResultStorage-${VERSION}.${REVISION}
	cp -rf ${PACKAGE_FILES} build/package/TestResultStorage-${VERSION}.${REVISION}
	tar -C build/package -czf TestResultStorage-${VERSION}.${REVISION}.tar.gz TestResultStorage-${VERSION}.${REVISION}
	rm -rf build/package

rpm: dist
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	expr ${RELEASE} + 1 > RELEASE
	rpmbuild -ta TestResultStorage-${VERSION}.${REVISION}.tar.gz --clean --define "_topdir `pwd`/build" --define "REVISION ${REVISION}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}"
