#note: this package is better to build with python setup.py bdist_rpm

Summary: Plugins for nose
Name: insecticide
Version: 0.1
Release: 0
License : GPL
URL: http://www.loki.name/insecticide
Group: Developement / libraries
Source: insecticide-0.2.tar.gz
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
rm -rf $RPM_BUILD_ROOT
DESTDIR=$RPM_BUILD_ROOT make install

%clean
rm -rf %RPM_BUILD_ROOT

%files
/usr/local/lib/python2.5/site-packages/*