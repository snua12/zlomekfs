# Makefile
# Copyright (C) 2006 Petr Tuma
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of zlomekFS.
#
# zlomekFS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zlomekFS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with zlomekFS; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

.PHONY: all install uninstall clean

PACKAGE_FILES = conf  fuse  Makefile  module  proxy  tests  zfsd  zlomekfs.spec

INSTALL = install

PREFIX ?= /usr

VERSION ?= $(shell cat VERSION)

RELEASE ?= $(shell if [ -f "RELEASE" ]; then cat RELEASE; else echo 0; fi)

ifeq ($(REVISION),)
        REVISION:= $(shell svnversion | sed -e 's/\([0-9]*\).*/\1/')
endif
ifeq ($(REVISION),)
	REVISION = 0
endif

all: fuse/lib/.libs/libfuse.a
	@$(MAKE) -C zfsd
#	@$(MAKE) -C module

test: 
	@$(MAKE) -C tests test

fuse/lib/.libs/libfuse.a:
	sh -c "cd fuse && \
	./makeconf.sh && \
	./configure --prefix=$(PREFIX) && \
	make"

install: 
#	@$(MAKE) -C zfsd-status-py install
	@$(MAKE) -C zfsd install
#	@$(MAKE) -C module install
	@$(MAKE) -C fuse install
	@$(MAKE) -C conf install

uninstall:
	@$(MAKE) -C zfsd uninstall

clean: 
	@$(MAKE) -C zfsd clean
	@$(MAKE) -C tests clean
#	@$(MAKE) -C module clean
	@$(MAKE) -C zfsd-status-py clean
	@$(MAKE) -C fuse clean || true
	rm -rf zlomekfs-*.tar.gz build

package: clean
	mkdir -p build/package/zlomekfs-${VERSION}.${REVISION}
	cp -rf  ${PACKAGE_FILES} build/package/zlomekfs-${VERSION}.${REVISION}
	tar -C build/package -czf zlomekfs-${VERSION}.${REVISION}.tar.gz zlomekfs-${VERSION}.${REVISION}

rpm: package
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	expr ${RELEASE} + 1 > RELEASE
	rpmbuild -ta zlomekfs-${VERSION}.${REVISION}.tar.gz --clean --define "_topdir `pwd`/build" --define "REVISION ${REVISION}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}"
	
