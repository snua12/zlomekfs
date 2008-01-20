#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
//#include <stdlib.h>
extern void exit(int);

#define PATTERN_START	">/dev/null console out"

int main(int argc, char ** argv){
	if(argc < 3){
		fprintf(stderr, "%s <infile> <outfile>\n", argv[0]);
		exit(1);
	}
	// input file with raw accounting
	FILE * in = fopen(argv[1], "r");

	if(in == NULL){
		perror("can't open in file\n");
	}
	
	// output file with collumned accounting
	FILE * out = fopen(argv[2],"w");
	
	if(out == NULL){
		perror("can't open out file\n");
	}
	
	// get length of in
	fseek(in, 0, SEEK_END);
	long long flen = ftell(in);
	fseek(in, 0, SEEK_SET);

	// map it to memory
	void * inMemory = mmap(NULL, flen, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(in), 0);
	
	if(inMemory == NULL){
		perror("can't mmap in file\n");
	}

	char * current = (char*)inMemory;
	char * next = NULL;

	// loop, while there is some another record
	while((next = strstr(current, PATTERN_START)) != NULL){

		next = strstr(next, " mode ");
		next[6] = '5';
		current = next + 1;
	}

	fprintf(out, "%s", (char*)inMemory);
	fclose(in);
	fclose(out);
	return 0;
}
