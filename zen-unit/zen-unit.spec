
Summary: Minimalistic approach to unit testing (c, c++)
Name: zen-unit
Version: 0.1
Release: 0
License : GPL
URL: http://loki.name/zen-unit
Group: Developement/Libraries
Source: zen-unit-%{version}.tar.gz
Prefix: %{_prefix}
BuildPrereq: libelf0-devel doxygen
Requires: libelf0 libpthread.so.0
BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
Zen-unit is C/C++ unit testing library which make writing tests easier.


%package doc
Summary: Documentation for zen-unit (doxydoc)
Group: Documentation

%description doc
Html documentation for zen-unit.


%prep
%setup -q

%build
make lib doc

%install
rm -rf $RPM_BUILD_ROOT
DESTDIR=$RPM_BUILD_ROOT make install install-doc

%clean
rm -rf %RPM_BUILD_ROOT

%files
/usr/lib/libzen-unit.so
/usr/include/zen-unit.h
/usr/share/man/man3/zen-unit.h.3.gz
/usr/share/man/man3/ZEN_ASSERT.3.gz
/usr/share/man/man3/ZEN_TEST.3.gz


%files doc
/usr/share/doc/zen-unit/*
