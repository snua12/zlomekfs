#
#	Makefile
#
#


PREFIX ?= /usr/local

SWIG=swig


PROJECT_ROOT=`pwd`/..
VERSION ?= `cat VERSION`

PYTHON_INCLUDE=-I/usr/include/python2.5 -I.

#if [ -f "/usr/include/dbus-service-descriptors.h" ]; then
#	PYTHON_INCLUDE += -I$(PROJECT_ROOT)/zfsd
#fi

all: build

zfsd_status.i: generate_swig_def.sh
	./generate_swig_def.sh zfsd_status.i

zfsd_status_wrap.c: zfsd_status.i
	$(SWIG) -python zfsd_status.i 


build: setup.py zfsd_status_wrap.c
	./setup.py build

clean:
	rm -rf *.i *.so *.pyc build dist MANIFEST zfsd_status_wrap.c zfsd_status.py *.tar.gz

install: build
	if [ -z "${DESTDIR}${PPREFIX}"]; then \
		./setup.py install; \
	else \
		./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"; \
	fi

package: clean
	rm -rf /tmp/zfsd-status-build
	mkdir -p /tmp/zfsd-status-build/zfsd-status-${VERSION}
	cp ./* /tmp/zfsd-status-build/zfsd-status-${VERSION}
	tar -C /tmp/zfsd-status-build -czf zfsd-status-${VERSION}.tar.gz zfsd-status-${VERSION}
	rm -rf /tmp/zfsd-status-build

rpm: package
	rpmbuild -ta zfsd-status-${VERSION}.tar.gz
# EOB

