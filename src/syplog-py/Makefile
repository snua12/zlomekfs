#
#	Makefile
#
#	python ipc module by Walter de Jong <walter@heiho.net>
#


SWIG=swig
PYTHON_INCLUDE=-I/usr/include/python2.5 -I.
SYPLOG_INCLUDE=-I../syplog -I../syplog/media -I../syplog/control -I../syplog/formaters


LIBS=../syplog/libsyplog.a

all: pysyplog_wrap.c

pysyplog.i: generate_swig_def.sh
	./generate_swig_def.sh pysyplog.i

pysyplog_wrap.c: pysyplog.i $(LIBS)
	$(SWIG) -python pysyplog.i 


$(LIBS):
	sh -c 'CFLAGS="-fPIC -shared " make -C ../syplog'

clean:
	rm -f $(LIBS) pysyplog.i pysyplog.py pysyplog_wrap.c _pysyplog.so

install: setup.py
	./setup.py install

# EOB

