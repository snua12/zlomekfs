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

all: fuse/lib/.libs/libfuse.a
	@$(MAKE) -C syplog
	@$(MAKE) -C zfsd
	@$(MAKE) -C tests

	@$(MAKE) -C module

test: install
	true

fuse/lib/.libs/libfuse.a:
	sh -c "cd fuse && \
	./makeconf.sh && \
	./configure && \
	make"

install: 
	@$(MAKE) -C syplog-py install

	@$(MAKE) -C syplog
	@$(MAKE) -C zfsd-status-py install
	@$(MAKE) -C zfsd install
	@$(MAKE) -C module install

uninstall:
	@$(MAKE) -C zfsd uninstall

clean:
	@$(MAKE) -C syplog clean
	@$(MAKE) -C zfsd clean
	@$(MAKE) -C module clean
	@$(MAKE) -C syplog-py clean
	@$(MAKE) -C zfsd-status-py clean
	@$(MAKE) -C tests clean
	rm -rf fuse/lib/.libs

