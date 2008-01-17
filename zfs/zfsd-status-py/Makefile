#
#	Makefile
#
#


SWIG=swig
PYTHON_INCLUDE=-I/usr/include/python2.5 -I.
SYPLOG_INCLUDE=-I../zfsd



all: zfsd_status_wrap.c

zfsd_status.i: generate_swig_def.sh
	./generate_swig_def.sh zfsd_status.i

zfsd_status_wrap.c: zfsd_status.i
	$(SWIG) -python zfsd_status.i 


clean:
	rm -rf *.i *.so *.pyc build

install: setup.py zfsd_status_wrap.c
	./setup.py install

# EOB

