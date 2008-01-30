# we build noarch package so we want to ignore pyo and pyc files
%define _unpackaged_files_terminate_build 0 
#note: this package is better to build with python setup.py bdist_rpm

Summary: Plugins for nose
Name: insecticide
Version: %{VERSION}.%{REVISION}
Release: 0
License : GPL
URL: http://www.loki.name/insecticide
Group: Developement / libraries
Source: insecticide-%{version}.tar.gz
ExclusiveOS: linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildArch: noarch
Prefix: %{_prefix}
#TODO: switch kernel-source and kernel-devel according distro
Requires: TestResultStorage python 

%description
plugins

%prep
%setup -q

%build
make all

%install
rm -rf %{buildroot}
python setup.py install --single-version-externally-managed --root=%{buildroot} --no-compile --record=INSTALLED_FILES

%clean
rm -rf %{buildroot}

%files -f INSTALLED_FILES
%defattr(-,root,root)
