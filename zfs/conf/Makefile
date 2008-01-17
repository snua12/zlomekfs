
INSTALL = install -g root -o root -m 444

# configuration and var is only one :)
PREFIX = ""

install:
	mkdir -p $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d
	mkdir -p $(DESTDIR)/$(PREFIX)/etc/zfs
	mkdir -p $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/group
	mkdir -p $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/user
	mkdir -p $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/volume
	mkdir -p $(DESTDIR)/$(PREFIX)/var/zfs/zfs_root

	$(INSTALL) etc/dbus-1/system.d/zfsd.conf -t $(DESTDIR)/$(PREFIX)/etc/dbus-1/system.d
	$(INSTALL) etc/zfs/* -t $(DESTDIR)/$(PREFIX)/etc/zfs
	$(INSTALL) -m 444 root_node/zfs_config/group/default -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 root_node/zfs_config/group/my_great_node -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 root_node/zfs_config/group_list -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/node_list -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/user/default -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 root_node/zfs_config/user/my_great_node -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 root_node/zfs_config/user_list -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/volume/my_tree -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config/volume
	$(INSTALL) -m 444 root_node/zfs_config/volume_list -t $(DESTDIR)/$(PREFIX)/var/zfs/zfs_config


uninstall:

