Summary: Distributed file system
Name: zlomekfs
Version: 0.0
Release: 0
License : GPL
URL: http://dsrg.mff.cuni.cz/~ceres/prj/zlomekFS
Group: System Environment/Daemons
Source: zlomekfs-0.0.tar.gz
Prefix: %{_prefix}
#TODO: switch kernel-source and kernel-devel according distro
BuildPrereq: dbus-1-devel syplog kernel-source
Requires: dbus-1 libpthread.so.0 syplog

%description
Distributed file system

%prep
%setup -q

%build
make all

%install
rm -rf $RPM_BUILD_ROOT
DESTDIR=$RPM_BUILD_ROOT make install

%clean
rm -rf %RPM_BUILD_ROOT

%files
/dev/fuse
/etc/dbus-1/system.d/zfsd.conf
/etc/udev/rules.d/99-fuse.rules
/etc/zfs/config
/etc/zfs/this_node
/etc/zfs/volume_info
/sbin/mount.fuse
/usr/bin/dump-intervals
/usr/bin/dump-metadata
/usr/include/dbus-service-descriptors.h
/usr/sbin/zfsd
/var/zfs/zfs_config/group_list
/var/zfs/zfs_config/user_list
/var/zfs/zfs_config/volume_list
/var/zfs/zfs_config/group/*
/var/zfs/zfs_config/user/*
/var/zfs/zfs_config/volume/*
/var/zfs/zfs_root
