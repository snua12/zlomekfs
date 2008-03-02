Summary: Full-featured remote controllable log
Name: syplog
Version: %{VERSION}
Release: %{REVISION}.%{RELEASE}
License : GPL
URL: http://loki.name/syplog
Group: Developement/Libraries
Source: syplog-%{version}.tar.gz
Prefix: %{_prefix}
BuildPrereq: dbus-devel doxygen automake autoconf libtool gettext gettext-devel
Requires: dbus libpthread.so.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Syplog is C based logging library.
Features:
- facilities
- log levels
- plugable targets
- plugable formatting
- easy configurable
- remotely controllable

%package doc
Summary: Documentation for syplog (doxydoc)
Group: Documentation

%description doc
Html documentation for syplog


%prep
%setup -q

%build
./configure --prefix=%{prefix}
make all doc

%install
rm -rf $RPM_BUILD_ROOT
DESTDIR=%{buildroot} make install install-doc
rm -rf %{buildroot}/%{_libdir}/libsyplog.la

%clean
rm -rf %RPM_BUILD_ROOT

%files
%{_libdir}/libsyplog.so
%{_libdir}/libsyplog.so.0
%{_libdir}/libsyplog.so.0.0.0
%{_libdir}/libsyplog.a
%{_includedir}/syplog/*
%config /etc/dbus-1/system.d/syplog.conf

%files doc
%docdir /usr/share/doc/syplog/
/usr/share/doc/syplog/