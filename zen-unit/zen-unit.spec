Summary: Minimalistic approach to unit testing (c, c++)
Name: zen-unit
Version: %{VERSION}
Release: %{REVISION}.%{RELEASE}
License : GPL
URL: http://loki.name/zen-unit
Group: Developement/Libraries
Source: zen-unit-%{version}.tar.gz
Exclusiveos: linux
Prefix: %{_prefix}
BuildRequires: libelf0-devel doxygen automake autoconf autoheader libtool gettext gettext-devel
Requires: libelf0 libpthread.so.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

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
./configure --prefix=%{prefix}
make all doc

%install
rm -rf %{buildroot}
DESTDIR=%{buildroot} make install install-doc
rm -rf %{buildroot}%{_libdir}/libzenunit.la

%clean
rm -rf %{buildroot}

%files
%{_libdir}/libzenunit.so
%{_libdir}/libzenunit.so.0
%{_libdir}/libzenunit.so.0.0.0
%{_includedir}/zen-unit.h
%{prefix}/share/man/man3/zen-unit.h.3.gz
%{prefix}/share/man/man3/ZEN_ASSERT.3.gz
%{prefix}/share/man/man3/ZEN_TEST.3.gz


%files doc
%docdir ${prefix}/share/doc/zen-unit/
%{prefix}/share/doc/zen-unit/

