all: preklapec graphMaker mode5

CFLAGS=-g3 -ggdb -O0 -Wall

preklapec: preklapec.c
	gcc ${CFLAGS} -o preklapec preklapec.c

graphMaker: graphMaker.c
	gcc ${CFLAGS} -lgd -o graphMaker graphMaker.c

mode5: mode5.c
	gcc ${CFLAGS} -o mode5 mode5.c

clean: 
	rm -rf *.o preklapec mode5 graphMaker