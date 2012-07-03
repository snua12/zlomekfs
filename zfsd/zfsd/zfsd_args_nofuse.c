/*
 * ! \file \brief daemon fuselike option parser
 */

#include <getopt.h>
#include "configuration.h"
#include <stdio.h>
#include <string.h>
#include "zfsd_args.h"
#include "zfs_config.h"

static struct option long_options[] =
{
	/* These options set a flag. */
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

static void process_o_args(char * o_args)
{
	char * arg;
	for (arg = strtok(o_args, ","); arg != NULL; arg = strtok(NULL, ","))
	{
		if (strncmp(arg, "config=", 7) == 0)
		{
			set_local_config_path(arg + 7); //7 is sizeof("config=") - 1
			continue;
		}

		if (strncmp(arg, "loglevel=", 9) == 0)
		{
			set_log_level(&syplogger, atoi(arg + 9)); //9 is sizeof("loglevel=") -1
			continue;
		}
	}
}

void process_arguments(int argc, char **argv)
{
	int option_index = 0;
	// resets get opt
	optind = 1;
	int c;

	while (1)
	{
		c = getopt_long(argc, argv, "o:hdf", long_options, &option_index);
		switch (c)
		{
			case -1:
				zfs_config.mountpoint = xstrdup(argv[optind]);
				optind = 1;
				return;
			case 'o':
				process_o_args(optarg);
				break;
			case 'd':
				break;
			case 'f':
				break;
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
			case 'v':
				version(EXIT_SUCCESS);
				exit(EXIT_SUCCESS);
			case '?':
				usage();
				exit(EXIT_FAILURE);
			default:
				usage();
				exit(EXIT_FAILURE);
		}

	};
}

void free_arguments(void)
{
}