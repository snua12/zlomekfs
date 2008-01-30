#
#	Makefile
#
#


PREFIX ?= /usr/local

SWIG=swig


PROJECT_ROOT=`pwd`/..
VERSION ?= `cat VERSION`

PACKAGE_FILES = generate_swig_def.sh Makefile setup.py VERSION zfsd_status.c zfsd_status.h zfsd-status.spec

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

test:
	python -c 'import zfsd_status'

clean:
	rm -rf *.i *.so *.pyc build dist MANIFEST zfsd_status_wrap.c zfsd_status.py *.tar.gz build

install: build
	if [ -z "${DESTDIR}${PPREFIX}"]; then \
		./setup.py install; \
	else \
		./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"; \
	fi

package: clean
	rm -rf build/package
	mkdir -p build/package/zfsd-status-${VERSION}
	cp ${PACKAGE_FILES} build/package/zfsd-status-${VERSION}
	tar -C build/package -czf zfsd-status-${VERSION}.tar.gz zfsd-status-${VERSION}
	rm -rf build/package

rpm: package
	mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
	rpmbuild -ta zfsd-status-${VERSION}.tar.gz --clean --define _topdir`pwd`/build
# EOB

