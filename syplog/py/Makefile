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



SWIG=swig

VERSION ?= $(shell cat VERSION)

RELEASE ?= $(shell if [ -f "RELEASE" ]; then cat RELEASE; else echo 0; fi)

ifeq ($(REVISION),)
        REVISION:= $(shell svnversion | sed -e 's/\([0-9]*\).*/\1/')
endif
ifeq ($(REVISION),)
	REVISION = 0
endif

PYTHON_INCLUDE=-I/usr/include/python2.5 -I.

#PROJECT_ROOT=`pwd`/..
#SYPLOG_INCLUDE=-I${PROJECT_ROOT}/src
#LIBS = ${PROJECT_ROOT}/src/libsyplog.so

PACKAGE_FILES = generate_swig_def.sh Makefile pysyplog.c pysyplog.spec setup.py VERSION

all: build

pysyplog.i: generate_swig_def.sh
	./generate_swig_def.sh pysyplog.i

pysyplog_wrap.c: pysyplog.i
	$(SWIG) -python pysyplog.i

test: 
	python -c 'import pysyplog'

build: pysyplog_wrap.c 
	python setup.py build


clean:
	rm -rf pysyplog.i pysyplog.py pysyplog_wrap.c _pysyplog.so build dist MANIFEST pysyplog-*.tar.gz pysyplog.egg-info build

install: setup.py build
	./setup.py install

dist: clean
	rm -rf build/package
	mkdir build/package/pysyplog-${VERSION} -p
	cp -rf ${PACKAGE_FILES} build/package/pysyplog-${VERSION}
	tar -C build/package -czf pysyplog-${VERSION}.tar.gz pysyplog-${VERSION}
	rm -rf build/package

rpm: dist
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	expr ${RELEASE} + 1 > RELEASE
	rpmbuild -ta pysyplog-${VERSION}.tar.gz --clean --define "_topdir `pwd`/build" --define "REVISION ${REVISION}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}"
