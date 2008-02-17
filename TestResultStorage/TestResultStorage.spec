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
Requires: python-django-snapshot python MySQL-python

%description
Repo

%prep
%setup -q

%build
make all

%install
rm -rf %{buildroot}
python setup.py install --record=INSTALLED_FILES --root=%{buildroot} --no-compile --single-version-externally-managed
DESTDIR=%{buildroot} make install-data

%clean
rm -rf %{buildroot}

%files -f INSTALLED_FILES
%config /usr/lib/python2.5/site-packages/TestResultStorage/settings.py
/var/lib/TestResultStorage/templates/resultRepository/default_page.html
/var/lib/TestResultStorage/templates/resultRepository/batchrun_detail.html
/var/lib/TestResultStorage/templates/resultRepository/batchrun_list.html
/var/lib/TestResultStorage/templates/resultRepository/testrun_detail.html
/var/lib/TestResultStorage/templates/resultRepository/testrun_list.html
/var/lib/TestResultStorage/templates/resultRepository/project_list.html
/var/lib/TestResultStorage/webMedia/style.css
%dir /var/lib/TestResultStorage/data

%defattr(-,root,root)
