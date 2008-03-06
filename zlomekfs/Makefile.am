# Makefile
# Copyright (C) 2006 Petr Tuma
# Copyright (C) 2008 Jiri Zouhar
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


EXTRA_DIST = \
	zlomekfs.spec \
	Doxyfile \
	conf/etc/dbus-1/system.d/zfsd.conf \
	conf/etc/zfs/config \
	conf/etc/zfs/this_node \
	conf/etc/zfs/volume_info \
	conf/root_node/zfs_config/group_list \
	conf/root_node/zfs_config/node_list \
	conf/root_node/zfs_config/user_list \
	conf/root_node/zfs_config/volume_list \
	conf/root_node/zfs_config/group/default \
	conf/root_node/zfs_config/group/my_great_node \
	conf/root_node/zfs_config/user/default \
	conf/root_node/zfs_config/user/my_great_node \
	conf/root_node/zfs_config/volume/my_tree \
	conf/root_node/zfs_root


rpm: dist
	./make-rpm.sh

test:
	@$(MAKE) -C tests test
	@$(MAKE) -C zfsd test

doc: 
	doxygen

html: doc

install-doc: doc
	mkdir -p $(DESTDIR)/$(prefix)/share/doc/zlomekfs
	$(INSTALL) -m 444 doc/html/* -t $(DESTDIR)/$(prefix)/share/doc/zlomekfs

install-exec-hook:
	mkdir -p $(DESTDIR)/etc/dbus-1/system.d
	mkdir -p $(DESTDIR)/etc/dbus-1/system.d
	mkdir -p $(DESTDIR)/etc/zfs
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/group
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/user
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/volume
	mkdir -p $(DESTDIR)/var/zfs/zfs_root

	$(INSTALL) conf/etc/dbus-1/system.d/zfsd.conf -t $(DESTDIR)/etc/dbus-1/system.d
	$(INSTALL) conf/etc/zfs/* -t $(DESTDIR)/etc/zfs
	$(INSTALL) -m 444 conf/root_node/zfs_config/group/default -t $(DESTDIR)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 conf/root_node/zfs_config/group/my_great_node -t $(DESTDIR)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 conf/root_node/zfs_config/group_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 conf/root_node/zfs_config/node_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 conf/root_node/zfs_config/user/default -t $(DESTDIR)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 conf/root_node/zfs_config/user/my_great_node -t $(DESTDIR)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 conf/root_node/zfs_config/user_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 conf/root_node/zfs_config/volume/my_tree -t $(DESTDIR)/var/zfs/zfs_config/volume
	$(INSTALL) -m 444 conf/root_node/zfs_config/volume_list -t $(DESTDIR)/var/zfs/zfs_config

install-html: install-doc

clean-local:
	rm -rf build doxy-warnings


SUBDIRS = \
	fuse \
	zfsd \
	tests

