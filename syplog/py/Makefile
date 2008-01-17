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

PROJECT_ROOT=`pwd`/..

PYTHON_INCLUDE=-I/usr/include/python2.5 -I.
SYPLOG_INCLUDE=-I${PROJECT_ROOT}/src



LIBS=${PROJECT_ROOT}/src/libsyplog.so

all: build

pysyplog.i: generate_swig_def.sh
	./generate_swig_def.sh pysyplog.i

pysyplog_wrap.c: pysyplog.i
	$(SWIG) -python pysyplog.i

build: pysyplog_wrap.c ${LIBS}
	python setup.py build

$(LIBS):
	make -C ${PROJECT_ROOT}/src libsyplog.so

clean:
	rm -rf $(LIBS) pysyplog.i pysyplog.py pysyplog_wrap.c _pysyplog.so build dist MANIFEST

install: setup.py build
	./setup.py install

# EOB

