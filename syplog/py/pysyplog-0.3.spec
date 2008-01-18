
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
BuildRequires: dbus-devel
Requires: python syplog dbus

%description
see syplog

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
