
all: libsyplog.a

CFLAGS += -I`pwd` -Wall -ggdb -g3 -pthread -DDEBUG -O0 -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

OO_SYPLOG = syplog.o \
	formatters/formatter.o \
	settings-loader.o \
	formatters/user-readable-formatter.o \
	formatters/raw-formatter.o \
	media/medium.o \
	media/file-medium.o \
	media/shm-medium.o \
	media/medium-api.o \
	syp-error.o \
	control/control-protocol.o \
	control/listener.o \
	control/control.o

libsyplog.a: $(OO_SYPLOG)
	ar -cru -o $@ $^


clean:
	rm *.o media/*.o formatters/*.o control/*.o libsyplog.a -f
