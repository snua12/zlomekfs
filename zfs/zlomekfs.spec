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
%config /etc/dbus-1/system.d/zfsd.conf
%config /etc/udev/rules.d/99-fuse.rules
%config /etc/zfs/config
%config /etc/zfs/this_node
%config /etc/zfs/volume_info
/sbin/mount.fuse
/usr/bin/dump-intervals
/usr/bin/dump-metadata
/usr/include/zfsd/dbus-service-descriptors.h
/usr/sbin/zfsd
%config /var/zfs/zfs_config/group_list
%config /var/zfs/zfs_config/user_list
%config /var/zfs/zfs_config/volume_list
%config /var/zfs/zfs_config/node_list
%config /var/zfs/zfs_config/group/*
%config /var/zfs/zfs_config/user/*
%config /var/zfs/zfs_config/volume/*
%config /var/zfs/zfs_root
