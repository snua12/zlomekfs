
#note: this package is better to build through python setup.py bdist_rpm

Summary: Repository for test results
Name: TestResultStorage
Version: 0.1
Release: 0
License : GPL
URL: http://www.loki.name/TestResultStorage
Group: System Environment/Daemons
Source: TestResultStorage-%{version}.tar.gz
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
python setup.py install --record=INSTALLED_FILES --single-version-externally-managed

%clean
rm -rf $RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
