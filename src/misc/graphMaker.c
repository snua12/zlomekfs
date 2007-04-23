
/* Bring in gd library functions */
#include "gd.h"

/* Bring in standard I/O so we can output the PNG to a file */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>

typedef unsigned long long ullong;
typedef unsigned int uint;

/* Declare the image */

gdImagePtr im;

/* Declare output files */

FILE *pngout;

/* Declare color indexes */

int black;
int mode0;
int mode1;
int mode2;
int mode3;
int mode4;
int mode5;
int white;

int width = 640;
int height = 480;
int borders = 50;
int markerWidht = 50;

typedef struct point{
	unsigned int timeSpent;
	unsigned int charsPrinted;
	unsigned int count;
	struct point * prev;
	struct point * next;
} pointStruct;

typedef struct modeData{
	pointStruct * begin;
	int mode;
	unsigned long long sum;
	unsigned int count;
	unsigned int maxTime, maxCharsPrinted;
	unsigned long long timeSum, charsSum;
	char outputName[255];
} modeDataStruct;

int maxTime = 0;
int maxCharsPrinted = 0;

int max(int x, int y){
	return x>y ? x : y;
}

modeDataStruct mode0Data = {
	.begin = NULL,
	.mode = 0,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "Discard",
};
modeDataStruct mode1Data = {
	.begin = NULL,
	.mode = 1,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "Memory",
};
modeDataStruct mode2Data = {
	.begin = NULL,
	.mode = 2,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "SHM",
};
modeDataStruct mode3Data = {
	.begin = NULL,
	.mode = 3,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "File",
};
modeDataStruct mode4Data = {
	.begin = NULL,
	.mode = 4,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "Console",
};

modeDataStruct mode5Data = {
	.begin = NULL,
	.mode = 4,
	.sum = 0,
	.count = 0,
	.maxTime = 0,
	.maxCharsPrinted = 0,
	.timeSum = 0,
	.charsSum = 0,
	.outputName = "ConsoleNulled",
};

pointStruct * newPoint(int timeSpent, int charsPrinted){
	pointStruct * ret = malloc(sizeof(pointStruct));
	if(ret == NULL){
		return NULL;
	}

	ret->prev = ret->next = NULL;
	ret->charsPrinted = charsPrinted;
	ret->timeSpent = timeSpent;
	ret->count = 1;

	return ret;
}


void appendNode(int mode, int timeSpent, int charsPrinted){
	modeDataStruct * actualMode = NULL;

	switch(mode){
		case -1:
		case 0:
			actualMode = & mode0Data;
			break;
		case 1:
			actualMode = & mode1Data;
			break;
		case 2:
			actualMode = & mode2Data;
			break;
		case 3:
			actualMode = & mode3Data;
			break;
		case 4:
			actualMode = & mode4Data;
			break;
		case 5:
			actualMode = & mode5Data;
		default:
			fprintf(stderr, "unexpected mode %d found\n",mode);
			break;
	}

	pointStruct * point = newPoint(timeSpent, charsPrinted);
	if(point == NULL){
		fprintf(stderr, "out of memory\n");
		exit(1);
	}


	actualMode->timeSum += point->timeSpent;
	actualMode->charsSum += point->charsPrinted;

	if(actualMode->begin ==NULL){
		actualMode->begin = point;
		actualMode->sum = timeSpent;
		actualMode->count = 1;
		actualMode->maxTime = timeSpent;
		actualMode->maxCharsPrinted = charsPrinted;
	}else{
		if(actualMode->maxTime < timeSpent){
			actualMode->maxTime = timeSpent;
		}
		if(actualMode->maxCharsPrinted < charsPrinted){
			actualMode->maxCharsPrinted = charsPrinted;
		}

		actualMode->sum += timeSpent;
		actualMode->count++;
		pointStruct * where = actualMode->begin;
		while(where->charsPrinted < point->charsPrinted && where->next != NULL){
			where = where->next;
		}

		if(where->charsPrinted < point->charsPrinted){// append to end
			where->next = point;
			point->prev = where;
		}else if (where == actualMode->begin){// insert on begining
			actualMode->begin = point;
			point->next = where;
			where->prev = point;
		}else{// somewhere in the middle
			point->next = where;
			point->prev = where->prev;
			point->prev->next = point;
			point->next->prev = point;
		}
	}
	
}

int normalizeWidth(int x){
//	return x / ( maxCharsPrinted / (width - borders*2) );

	int printWidth = width - 2*borders;
	int dataWidth = maxCharsPrinted - 0;
	int normalizedPoint = ((x * printWidth) / dataWidth);
	

	return borders + normalizedPoint ;
}

int normalizeHeight(int y){
	int printHeight = height - 2*borders;
	int dataHeight = maxTime - 0;
	int normalizedPoint = ((y * printHeight) / dataHeight);

	return height - borders - normalizedPoint;
}

void loadData(char * infile){
// input file with raw accounting
	FILE * in = fopen(infile, "r");

	if(in == NULL){
		perror("can't open in file\n");
	}
	
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
	ullong user, system, overal;
	uint  cpu, majorPageFaults, minorPageFaults;

	char * current = (char*)inMemory;
	char * next = NULL;

	// loop, while there is some another record
	current += strlen("Verbosity;Mode;TimeSpent;CharsPrinted;User;System;Overal;CPU;MajorFaults;MinorFaults;");
	while((next = strchr(current, '\n') + 1) != '\0' && next != (char*)0x1){

		sscanf(next, "%d;%d;%d;%d;%llu;%llu;%llu;%u;%u;%u;", 
			&verbosity, &mode,
			&timeSpent,
			&charsPrinted,
			&user, &system, &overal, &cpu,
			&majorPageFaults, &minorPageFaults
			);

		fprintf(stderr, "caught point %d/%d@%d\n", 
			charsPrinted, timeSpent, mode);
		appendNode(mode, timeSpent, charsPrinted);
		current = next;
	}

	maxTime = max(mode1Data.maxTime,
			max(mode2Data.maxTime,
				max(mode3Data.maxTime, 
					max(mode4Data.maxTime, 
						max(mode5Data.maxTime, mode0Data.maxTime)
		)	)	)	);

	maxCharsPrinted = max(mode1Data.maxCharsPrinted,
				max(mode2Data.maxCharsPrinted,
					max(mode3Data.maxCharsPrinted, 
						max(mode4Data.maxCharsPrinted,
							max(mode5Data.maxCharsPrinted, 
								mode0Data.maxCharsPrinted)
			)	)		)	);

	fprintf(stderr, "maxTime=%d, maxChars=%d. nTime=%d, nChars=%d\n",
			maxTime, maxCharsPrinted,
			normalizeHeight(maxTime), normalizeWidth(maxCharsPrinted)
	);
}



void drawNormalizedPoint(int x, int y, int color){
	fprintf(stderr, "(%d, %d)->(%d, %d)\n",
			x, y, normalizeWidth(x), normalizeHeight(y)
		);
	gdImageSetPixel(im, normalizeWidth(x), normalizeHeight(y), color);

}

void drawNormalizedLine(int x, int y, int xx, int yy, int color){
	fprintf(stderr, "(%d,%d-%d,%d)->(%d,%d-%d,%d)\n",
			x, y, xx, yy,
			normalizeWidth(x), normalizeHeight(y),
			normalizeWidth(xx), normalizeHeight(yy)
		);
	gdImageLine(im, normalizeWidth(x), normalizeHeight(y),
		normalizeWidth(xx), normalizeHeight(yy), color);
}

void drawAxis(){
	gdImageLine(im, borders, borders - 10, borders, height - borders + 5, black);
	gdImageLine(im, borders -5, height - borders, width - 10, height - borders, black);
}

void drawLegend(){

}

void drawPoints(pointStruct * actual, int color){
	while(actual != NULL){
		drawNormalizedPoint(actual->charsPrinted, actual->timeSpent, color);
		actual = actual->next;
	}
}

void drawLine(pointStruct * actual, int color){

	while(actual != NULL && actual->next != NULL){
		drawNormalizedLine(actual->charsPrinted, actual->timeSpent,
			actual->next->charsPrinted, actual->next->timeSpent,
			color);
		actual = actual->next;
	}
	if(actual){
		drawNormalizedLine(actual->charsPrinted, actual->timeSpent,
			actual->charsPrinted, actual->timeSpent,
			color);

	}

}

#define TRUE	1
#define	FALSE	2

int isTheSame (pointStruct * one, pointStruct * two){

	if(one->charsPrinted == two->charsPrinted &&
		one->timeSpent == two->timeSpent)
		return TRUE;
	return FALSE;
}

void sumarizeMode(pointStruct * rowBegin){
	unsigned long long actualSum = 0;
	unsigned int actualNum = 0;
	pointStruct * rowEnd = rowBegin;
	pointStruct * deleter = NULL;

	// loop around all points
	while(rowBegin!=NULL){
		// loop while row
		while(rowEnd->next != NULL && 
			rowEnd->charsPrinted == rowEnd->next->charsPrinted){
			rowEnd = rowEnd->next;
		}

		// accumulate and free
		while(rowEnd != rowBegin){
			actualSum +=rowEnd->timeSpent;
			actualNum ++;
			if(rowEnd->next != NULL)
				rowEnd->next->prev = rowEnd->prev;
			if(rowEnd->prev != NULL)
				rowEnd->prev->next = rowEnd->next;
			deleter = rowEnd;
			rowEnd = rowEnd->prev;
			free(deleter);
		}

		actualSum += rowBegin->timeSpent;
		actualNum ++;
		rowBegin->timeSpent = actualSum / actualNum;
		rowBegin->count = actualNum;

		rowBegin = rowBegin->next;
		rowEnd = rowBegin;
		actualNum = 0;
		actualSum = 0;
	}
	
}

void sumarizeData(){
	sumarizeMode(mode0Data.begin);
	sumarizeMode(mode1Data.begin);
	sumarizeMode(mode2Data.begin);
	sumarizeMode(mode3Data.begin);
	sumarizeMode(mode4Data.begin);
	sumarizeMode(mode5Data.begin);
}



void printNodes(FILE* out, pointStruct * begin){
	for(; begin != NULL; begin = begin->next){
		fprintf(out, "count:%d\tchars:%d\ttime:%d\n",
			begin->count, begin->charsPrinted, begin->timeSpent);
	}
}

void printModes(FILE* out){
	fprintf(out, "=== data for mode %s ===\n", mode0Data.outputName);
	printNodes(out, mode0Data.begin);
	fprintf(out, "=== data for mode %s ===\n", mode1Data.outputName);
	printNodes(out, mode1Data.begin);
	fprintf(out, "=== data for mode %s ===\n", mode2Data.outputName);
	printNodes(out, mode2Data.begin);
	fprintf(out, "=== data for mode %s ===\n", mode3Data.outputName);
	printNodes(out, mode3Data.begin);
	fprintf(out, "=== data for mode %s ===\n", mode4Data.outputName);
	printNodes(out, mode4Data.begin);
	fprintf(out, "=== data for mode %s ===\n", mode5Data.outputName);
	printNodes(out, mode5Data.begin);
}

void drawGraph(){


	fprintf(stderr, "===== drawing mode0 =====\n");
	drawPoints(mode0Data.begin, mode0);

	fprintf(stderr, "===== drawing mode2 =====\n");
	drawPoints(mode1Data.begin, mode1);

	fprintf(stderr, "===== drawing mode3 =====\n");
	drawPoints(mode2Data.begin, mode2);

	fprintf(stderr, "===== drawing mode4 =====\n");
	drawPoints(mode3Data.begin, mode3);

	fprintf(stderr, "===== drawing mode4 =====\n");
	drawPoints(mode4Data.begin, mode4);

	fprintf(stderr, "===== drawing mode5 =====\n");
	drawPoints(mode4Data.begin, mode5);

}


int main(int argc, char ** argv) {

if(argc<2){
	fprintf(stderr, "too few arguments\n");
	fprintf(stderr, "%s <infile> <graphfile> [summaryfile]\n", argv[0]);
}

im = gdImageCreate(width, height);



/* Allocate the color white
Since this is the first color in a new image, it will

be the background color. */
white = gdImageColorAllocate(im, 255, 255, 255);

black = gdImageColorAllocate(im, 0, 0, 0);
mode0 = gdImageColorAllocate(im, 0, 0, 0);
mode1 = gdImageColorAllocate(im, 255, 0, 0);
mode2 = gdImageColorAllocate(im, 0, 0, 255);
mode3 = gdImageColorAllocate(im, 0, 255, 0);
mode4 = gdImageColorAllocate(im, 128, 128, 128);
mode5 = gdImageColorAllocate(im, 150, 0, 150);  

loadData(argv[1]);
drawAxis();
drawLegend();
drawGraph();


// write output
pngout = fopen(argv[2], "wb");
gdImagePng(im, pngout);
fclose(pngout);


/* Destroy the image in memory. */
gdImageDestroy(im);

if(argc>3){
	FILE *  summaries = fopen(argv[3],"w");
	sumarizeData();
	printModes(summaries);
}

width = width / 2;
height = height / 2;

im = gdImageCreate(width, height);


white = gdImageColorAllocate(im, 255, 255, 255);

black = gdImageColorAllocate(im, 0, 0, 0);
mode0 = gdImageColorAllocate(im, 0, 0, 0);
mode1 = gdImageColorAllocate(im, 255, 0, 0);
mode2 = gdImageColorAllocate(im, 0, 0, 255);
mode3 = gdImageColorAllocate(im, 0, 255, 0);
mode4 = gdImageColorAllocate(im, 128, 128, 128);
mode5 = gdImageColorAllocate(im, 150, 0, 150);  

drawAxis();
drawLegend();
drawGraph();

char sumarizedName[255];
snprintf(sumarizedName,255,"%s.sumarized.png",argv[2]);
// write output
pngout = fopen(sumarizedName, "wb");
gdImagePng(im, pngout);
fclose(pngout);


/* Destroy the image in memory. */
gdImageDestroy(im);


return 0;

}
