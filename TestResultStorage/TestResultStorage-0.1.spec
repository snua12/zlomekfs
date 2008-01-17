
#note: this package is better to build through python setup.py bdist_rpm

Summary: Repository for test results
Name: TestResultStorage
Version: 0.1
Release: 0
License : GPL
URL: http://www.loki.name/TestResultStorage
Group: System Environment/Daemons
Source: TestResultStorage-0.1.tar.gz
Prefix: %{_prefix}
#TODO: switch kernel-source and kernel-devel according distro
Requires: django python

%description
Repo

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
/usr/local/lib/python2.5/site-packages/*