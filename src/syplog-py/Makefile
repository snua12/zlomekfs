#
#	Makefile
#
#	python ipc module by Walter de Jong <walter@heiho.net>
#

CC=gcc
COPT=-g3 -ggdb -O0 #-O2 -Winline
CFLAGS=$(COPT) -Wall
CFLAGSWRAP = $(COPT) -fPIC 
CFLAGSSO = $(COPT) -shared

SWIG=swig
PYTHON_INCLUDE=-I/usr/include/python2.5 -I.
SYPLOG_INCLUDE=-I../syplog -I../syplog/media -I../syplog/control -I../syplog/formaters


OBJS=pysyplog_wrap.o 
LIBS=../syplog/libsyplog.a

all: _pysyplog.so

pysyplog.i: generate_swig_def.sh
	./generate_swig_def.sh pysyplog.i

_pysyplog.so: $(OBJS) $(LIBS)
	$(CC) $(CFLAGSSO) $(OBJS) -o _pysyplog.so $(LIBS)

pysyplog_wrap.o: pysyplog_wrap.c
	$(CC) $(CFLAGSWRAP) $(PYTHON_INCLUDE) $(SYPLOG_INCLUDE) -c pysyplog_wrap.c -Dbool=char

pysyplog_wrap.c: pysyplog.i 
	$(SWIG) -python pysyplog.i 

.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBS):
	sh -c 'CFLAGS="-fPIC -shared " make -C ../syplog'

clean:
	rm -f $(OBJS) $(LIBS) pysyplog.i pysyplog.py pysyplog_wrap.c _pysyplog.so

# EOB

