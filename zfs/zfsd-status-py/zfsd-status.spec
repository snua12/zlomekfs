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
python setup.py install --single-version-externally-managed --record=INSTALLED_FILES

%clean
rm -rf %RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
