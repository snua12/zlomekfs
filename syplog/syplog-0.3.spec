Summary: Full-featured remote controllable log
Name: syplog
Version: 0.3
Release: 0
License : GPL
URL: http://loki.name/syplog
Group: Developement/Libraries
Source: syplog-0.3.tar.gz
Prefix: %{_prefix}
BuildPrereq: dbus-1-devel
Requires: dbus-1 libpthread.so.0

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
make lib doc

%install
rm -rf $RPM_BUILD_ROOT
DESTDIR=$RPM_BUILD_ROOT make install install-doc

%clean
rm -rf %RPM_BUILD_ROOT

%files
/usr/lib/lib*.so
/usr/lib/lib*.a
/usr/include/syplog/*
/etc/dbus-1/system.d/syplog.conf

%files doc
/usr/share/doc/syplog/*
