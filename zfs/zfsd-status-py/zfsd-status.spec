Summary: Zfsd-dbus service client
Name: zfsd-status
Version: %{VERSION}.%{REVISION}
Release: 0
License : GPL
URL: http://dsrg.mff.cuni.cz/~ceres/prj/zlomekFS
Group: System Environment/Daemons
Source: zfsd-status-%{version}.tar.gz
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Exclusiveos: linux
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
rm -rf %{buildroot}
python setup.py install --single-version-externally-managed --record=INSTALLED_FILES --root=%{buildroot}

%clean
rm -rf %{buildroot}

%files -f INSTALLED_FILES
/usr/lib/python2.5/site-packages/zfsd_status.pyo
%defattr(-,root,root)
