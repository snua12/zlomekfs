# we build noarch package so we want to ignore pyo and pyc files
%define _unpackaged_files_terminate_build 0 
#note: this package is better to build through python setup.py bdist_rpm

Summary: Python wrapper for syplog
Name: pysyplog
Version: 0.3
Release: 0
License : GPL
URL: http://www.loki.name/TestResultStorage
Group: Developement/Libraries
Source: pysyplog-%{version}.tar.gz
Prefix: %{_prefix}
ExclusiveOS: linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildArch: noarch

BuildRequires: dbus-devel
Requires: python syplog dbus

%description
see syplog

%prep
%setup -q

%build
make all

%install
rm -rf %{buildroot}
python setup.py install --record=INSTALLED_FILES --root=%{buildroot} --no-compile --single-version-externally-managed

%clean
rm -rf %{buildroot}

%files -f INSTALLED_FILES
%defattr(-,root,root)
