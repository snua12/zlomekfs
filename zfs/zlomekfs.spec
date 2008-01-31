Summary: Distributed file system
Name: zlomekfs
Version: %{VERSION}.%{REVISION}
Release: 0
License : GPL
URL: http://dsrg.mff.cuni.cz/~ceres/prj/zlomekFS
Group: System Environment/Daemons
Source: zlomekfs-%{version}.tar.gz
ExclusiveOS: linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
Exclusiveos: linux
#TODO: switch kernel-source and kernel-devel according distro
BuildPrereq: dbus-devel syplog kernel-devel libtool autoconf automake gettext-devel
Requires: dbus libpthread.so.0 syplog gettext
# we don't want to install all fuse files
%define _unpackaged_files_terminate_build 0 
%description
Distributed file system

%prep
%setup -q

%build
make all

%install
rm -rf %{buildroot}
DESTDIR=%{buildroot} make install

%clean
rm -rf %{buildroot}

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
/usr/include/zfsd/dbus-service-descriptors.h
/usr/sbin/zfsd
/var/zfs/zfs_config/group_list
/var/zfs/zfs_config/user_list
/var/zfs/zfs_config/volume_list
/var/zfs/zfs_config/node_list
/var/zfs/zfs_config/group/*
/var/zfs/zfs_config/user/*
/var/zfs/zfs_config/volume/*
/var/zfs/zfs_root
