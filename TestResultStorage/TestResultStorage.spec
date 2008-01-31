# we build noarch package so we want to ignore pyo and pyc files
%define _unpackaged_files_terminate_build 0
#note: this package is better to build through python setup.py bdist_rpm


Summary: Repository for test results
Name: TestResultStorage
Version: %{VERSION}.%{REVISION}
Release: 0
License : GPL
URL: http://www.loki.name/TestResultStorage
Group: System Environment/Daemons
Source: TestResultStorage-%{version}.tar.gz
ExclusiveOS: linux
Prefix: %{_prefix}
BuildArch: noarch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
#TODO: switch kernel-source and kernel-devel according distro
Requires: python-django-snapshot python

%description
Repo

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
