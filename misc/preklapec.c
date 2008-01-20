// reformat accounting info to some openOffice importable format
// TODO: discard devnulled

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
//#include <stdlib.h>
extern void exit(int);

#define PATTERN		"accounting for verbosity %d mode %d:\n" 				\
	"timeSpent: %d\n" 									\
	"charsPrinted: %d\n"									\
	"\n"											\
	"%llu.%lluuser %llu.%llusystem %llu:%llu.%lluelapsed %uCPU "				\
	"(%uavgtext+%uavgdata %umaxresident)k\n"						\
	"%uinputs+%uoutputs (%umajor+%uminor)pagefaults %uswaps"

#define PATTERN_START	"accounting for verbosity"

typedef unsigned long long ulong;
typedef unsigned int uint;

// print header for records printed by formatPrint
void printHeader(FILE * output){
	fprintf(output, "Verbosity;Mode;TimeSpent;CharsPrinted;"
			"User;System;Overal;"
			"CPU;MajorFaults;MinorFaults;"
			"\n");
}

// print out record in some format (currently with ';' as field delimiter and '\n' as record delimiter
void formatPrint(FILE * output, int verbosity, int mode, int timeSpent, int charsPrinted,
	ulong user, ulong system, ulong overal, uint cpu, uint majorFaults, uint minorFaults){

	fprintf(output, "%d;%d;%d;%d;"
			"%llu;%llu;%llu;"
			"%u;%u;%u;"
			"\n",
			verbosity, mode, timeSpent, charsPrinted,
			user, system, overal,
			cpu, majorFaults, minorFaults);
}

// merge min:sec:usec to usec
void usecify(ulong * usecs, ulong sec, ulong min){
	sec += 60 * min;
	*usecs += sec * 1000000;
}

// first arg infile second outfile
int main(int argc, char** argv){

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
	
	printHeader(out);

	// get length of in
	fseek(in, 0, SEEK_END);
	long long flen = ftell(in);
	fseek(in, 0, SEEK_SET);

	// map it to memory
	void * inMemory = mmap(NULL, flen, PROT_READ, MAP_PRIVATE, fileno(in), 0);
	
	if(inMemory == NULL){
		perror("can't mmap in file\n");
	}

	int verbosity, mode, timeSpent, charsPrinted;
	ulong userSec, userUSec, systemSec, systemUSec, overalMin, overalSec, overalUSec;
	uint  cpuLoad,	inputs, outputs, majorPageFaults, minorPageFaults, swaps;
	uint avgtext, avgdata, maxresident;

	char * current = (char*)inMemory;
	char * next = NULL;

	// loop, while there is some another record
	while((next = strstr(current, PATTERN_START)) != NULL){

		sscanf(next, PATTERN, 
			&verbosity, &mode,
			&timeSpent,
			&charsPrinted,
			&userSec, &userUSec, &systemSec, &systemUSec, &overalMin, &overalSec, &overalUSec, &cpuLoad,
			&avgtext, &avgdata, &maxresident,
			&inputs, &outputs, &majorPageFaults, &minorPageFaults, &swaps
			);
			
		if(inputs != 0 || outputs != 0 || swaps != 0){
			perror("on pos %d is non zero in/out/swap\n");
		}

		usecify(&userUSec, userSec, 0);
		usecify(&systemUSec, systemSec, 0);
		usecify(&overalUSec, overalSec, overalMin);

		formatPrint(out, verbosity, mode, timeSpent, charsPrinted,
				userUSec, systemUSec, overalUSec, 
				cpuLoad, majorPageFaults, minorPageFaults);

		current = next + 1;
	}
	
	

	// close streams
	munmap(inMemory, flen);
	fclose(in);
	fclose(out);

	return 0;
}
