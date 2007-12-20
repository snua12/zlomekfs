
all: libsyplog.a

CFLAGS += -I`pwd` -Wall -ggdb -g3 -pthread -DDEBUG

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

OO_SYPLOG = syplog.o \
	formaters/formater.o \
	settings-loader.o \
	formaters/user-readable-formater.o \
	formaters/raw-formater.o \
	media/medium.o \
	media/file-medium.o \
	media/shm-medium.o \
	media/medium-api.o \
	syp-error.o \
	control/control-protocol.o \
	control/listener.o

libsyplog.a: $(OO_SYPLOG)
	ar -cru -o $@ $^


clean:
	rm *.o media/*.o formaters/*.o control/*.o libsyplog.a -f
