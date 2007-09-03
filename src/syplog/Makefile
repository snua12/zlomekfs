
all: libsyplog.a

CFLAGS += -I`pwd` -Iformaters -Iwriters -Ireaders -Wall -ggdb -g3 -pthread -DDEBUG

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

OO_SYPLOG = syplog.o \
	writers/file-writer.o \
	writers/shm-writer.o \
	formaters/formater.o \
	settings-loader.o \
	formaters/user-readable-formater.o \
	formaters/raw-formater.o \
	writers/writer.o \
	readers/reader.o \
	readers/file-reader.o \
	readers/shm-reader.o \
	syp-error.o

libsyplog.a: $(OO_SYPLOG)
	ar -cru -o $@ $^


clean:
	rm *.o readers/*.o writers/*.o formaters/*.o libsyplog.a -rf
