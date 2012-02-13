#include <stdio.h>
#include "log.h"
#include "system.h"
#include "zfsd_args_shared.h"

// TODO: replace chars and options with DEFINES (use them on all
// places
void usage(void)
{
	fprintf(stdout, "Usage: zfsd [OPTION]...\n\n"
			"  -o config=FILE               "
			"Specifies the name of the configuration file.\n"
			"  -o loglevel=DEBUG_LEVEL      "
			"Display debugging messages up to level DEBUG_LEVEL.\n"
			"  --help                       "
			"Display this help and exit.\n"
			"  --version                    "
			"Output version information and exit.\n"
			"\n"
			"FUSE options:\n"
			"  -d, -o debug                 "
			"Enable debug output (implies -f)\n"
			"  -f                           "
			"Foreground operation\n");

	fflush(stdout);

	fprintf(stdout, "LOGGING OPTIONS:\n");
	print_syplog_help(1, 1);
}

/* 
 * ! Display the version, exit the program with exit code EXITCODE.  
 */
void ATTRIBUTE_NORETURN version(int exitcode)
{
	fprintf(stdout, "zfsd 0.1.0\n");
	fprintf(stdout, "Copyright (C) 2003, 2004 Josef Zlomek\n");
	fprintf
		(stdout, "This is free software; see the source for copying conditions.  There is NO\n");
	fprintf
		(stdout, "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	fflush(stdout);
	exit(exitcode);
}


