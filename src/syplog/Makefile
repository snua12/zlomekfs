
all: libsyplog.a

CC_OPTS += -I`pwd` -Iformaters -Iwriters -Wall -ggdb -g3 -pthread -DDEBUG

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

OO_SYPLOG = syplog.o \
	writers/file-writer.o \
	formaters/formater.o \
	settings-loader.o \
	formaters/user-readable-formater.o \
	formaters/raw-formater.o \
	writers/writer.o \
	syp-error.o

libsyplog.a: $(OO_SYPLOG)
	ar -cru -o $@ $^


clean:
	rm *.o writers/*.o formaters/*.o libsyplog.a -rf
