/*
 *! \file \brief daemon fuselike option parser
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
	char o_args_copy[8191];
	strncpy(o_args_copy, o_args, sizeof(o_args_copy) - 1);

	for (arg = strtok(o_args_copy, ","); arg != NULL; arg = strtok(NULL, ","))
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

#if defined(ENABLE_CLI_CONSOLE) && ! defined(__CYGWIN__)
// this function is from http://stackoverflow.com/questions/10543280/linux-daemon-tutorial
static bool daemon_seed() 
{
        int childpid = fork ();
        if (childpid  == -1) return false;

	if (childpid > 0) exit(0); //if we have a child then parent can exit

        //Set our sid and continue normal runtime as a forked process
        setsid ();

	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);

        return true;
}
#endif

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
				if (argv != NULL)
				{
					set_mountpoint(argv[optind]);
				}
				optind = 1;

				#if defined(ENABLE_CLI_CONSOLE) && ! defined(__CYGWIN__) //cli use console, don't daemonize
				daemon_seed();
				#endif
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
