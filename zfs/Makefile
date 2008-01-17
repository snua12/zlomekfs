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

INSTALL = install

PREFIX ?= /usr

all: fuse/lib/.libs/libfuse.a
	@$(MAKE) -C zfsd
#	@$(MAKE) -C module

test: all
	true

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
#	@$(MAKE) -C module clean
	@$(MAKE) -C zfsd-status-py clean
	@$(MAKE) -C fuse clean
	rm -rf zlomekfs-0.0.tar.gz

rpm: clean
	rm -rf /tmp/zlomekfs-build
	mkdir -p /tmp/zlomekfs-build/zlomekfs-0.0
	cp -rf  conf  fuse  Makefile  misc  module  proxy  tests  zfsd  zlomekfs-0.0.spec /tmp/zlomekfs-build/zlomekfs-0.0
	tar -C /tmp/zlomekfs-build -czf zlomekfs-0.0.tar.gz zlomekfs-0.0
	rpmbuild -ta zlomekfs-0.0.tar.gz
	
