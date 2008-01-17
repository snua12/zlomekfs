#
#	Makefile
#
#


PREFIX ?= /usr/local

SWIG=swig


PROJECT_ROOT=`pwd`/..

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
	rm -rf *.i *.so *.pyc build dist MANIFEST zfsd_status_wrap.c zfsd_status.py

install: build
	if [ -z "$DESTDIR$PREFIX"]; then \
		./setup.py install; \
	else \
		./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"; \
	fi

# EOB

