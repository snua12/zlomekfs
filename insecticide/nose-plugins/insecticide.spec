#note: this package is better to build with python setup.py bdist_rpm


Summary: Plugins for nose
Name: insecticide
Version: 0.2
Release: 0
License : GPL
URL: http://www.loki.name/insecticide
Group: Developement / libraries
Source: insecticide-%{version}.tar.gz
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
python setup.py install --single-version-externally-managed --record=INSTALLED_FILES
%clean
rm -rf %RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
