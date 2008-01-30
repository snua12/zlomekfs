
INSTALL = install 
#-g root -o root -m 444

# configuration and var is only one :)
PREFIX = ""

install:
	mkdir -p $(DESTDIR)/etc/dbus-1/system.d
	mkdir -p $(DESTDIR)/etc/zfs
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/group
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/user
	mkdir -p $(DESTDIR)/var/zfs/zfs_config/volume
	mkdir -p $(DESTDIR)/var/zfs/zfs_root

	$(INSTALL) etc/dbus-1/system.d/zfsd.conf -t $(DESTDIR)/etc/dbus-1/system.d
	$(INSTALL) etc/zfs/* -t $(DESTDIR)/etc/zfs
	$(INSTALL) -m 444 root_node/zfs_config/group/default -t $(DESTDIR)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 root_node/zfs_config/group/my_great_node -t $(DESTDIR)/var/zfs/zfs_config/group
	$(INSTALL) -m 444 root_node/zfs_config/group_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/node_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/user/default -t $(DESTDIR)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 root_node/zfs_config/user/my_great_node -t $(DESTDIR)/var/zfs/zfs_config/user
	$(INSTALL) -m 444 root_node/zfs_config/user_list -t $(DESTDIR)/var/zfs/zfs_config
	$(INSTALL) -m 444 root_node/zfs_config/volume/my_tree -t $(DESTDIR)/var/zfs/zfs_config/volume
	$(INSTALL) -m 444 root_node/zfs_config/volume_list -t $(DESTDIR)/var/zfs/zfs_config


uninstall:

