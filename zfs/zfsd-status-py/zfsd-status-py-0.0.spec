Summary: Zfsd-dbus service client
Name: zfsd-status
Version: 0.1
Release: 0
License : GPL
URL: http://dsrg.mff.cuni.cz/~ceres/prj/zlomekFS
Group: System Environment/Daemons
Source: zfsd-status-%{version}.tar.gz
Prefix: %{_prefix}
#TODO: switch kernel-source and kernel-devel according distro
BuildPrereq: dbus-devel zlomekfs
Requires: dbus libpthread.so.0

%description
Python wrapper for zfsd dbus service.

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
/usr/local/lib/python2.5/site-packages/zfsd_status-0.1-py2.5.egg-info  
/usr/local/lib/python2.5/site-packages/zfsd_status.pyc
/usr/local/lib/python2.5/site-packages/zfsd_status.py
/usr/local/lib/python2.5/site-packages/_zfsd_status.so
