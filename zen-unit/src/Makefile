LDFLAGS += -lelf --no-undefined -lpthread

CFLAGS += -pthread -I../include -fPIC -O0 -g3 -ggdb

all: libtao.so

TAO_SOURCES = \
	zen-search.o \
	zen-static-search.o \
	zen-shared-search.o \
	zen-elf-search.o \
	zen-library-run.o

libtao.so: ${TAO_SOURCES}
	gcc  ${LDFLAGS} \
	-shared -Wl,-soname,libtao.so.0 \
	-o libtao.so.0.0.1 ${TAO_SOURCES}
	ln -fs libtao.so.0.0.1 libtao.so
	ln -fs libtao.so.0.0.1 libtao.so.0

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o *.so libtao.so*
