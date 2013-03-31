/**
 *  \file zfsd_args_fuse.c
 * 
 *  \brief Implements command line parsing functions implementad by fuse_parse_cmdline
 *  \author Ales Snuparek
 *  \author Miroslav Trmac
 */

/*
 *! \file \brief daemon option parser
 */

#include "fs-iface.h"
#include "configuration.h"
#include "zfsd_args.h"

/* 
 *! For long options that have no equivalent short option, use a
 * non-character as a pseudo short option, starting with CHAR_MAX + 1. 
 */
enum
{
	OPTION_HELP,
	OPTION_VERSION,
};

struct zfs_opts
{
	char *config;
	int loglevel;
};

#define ZFS_OPT(t, p, v) { t, offsetof (struct zfs_opts, p), v }

static const struct fuse_opt main_options[] = {
	ZFS_OPT("config=%s", config, 0),
	ZFS_OPT("loglevel=%u", loglevel, DEFAULT_LOG_LEVEL),
	FUSE_OPT_KEY("--help", OPTION_HELP),
	FUSE_OPT_KEY("--version", OPTION_VERSION),
	FUSE_OPT_END
};

/* 
 *! Process command line arguments.  
 */
static int handle_one_argument(ATTRIBUTE_UNUSED void *data, const char *arg,
							   int key,
							   ATTRIBUTE_UNUSED struct fuse_args *outargs)
{
	if (is_logger_arg(arg) == TRUE)
		return 0;

	switch (key)
	{
	case OPTION_HELP:
		usage();
		exit(EXIT_SUCCESS);

	case OPTION_VERSION:
		version(EXIT_SUCCESS);
		exit(EXIT_SUCCESS);

	case FUSE_OPT_KEY_NONOPT:
	default:
		return 1;
	}
}


void process_arguments(int argc, char **argv)
{
	struct zfs_opts zopts;

	main_args = (struct fuse_args)FUSE_ARGS_INIT(argc, argv);
	memset(&zopts, 0, sizeof(zopts));
	if (fuse_opt_parse
		(&main_args, &zopts, main_options, handle_one_argument) != 0)
	{
		usage();
		exit(EXIT_FAILURE);
	}

	if (zopts.config)
	{
		set_local_config_path(zopts.config);
	}

	set_log_level(&syplogger, zopts.loglevel);


	// start zfsd on background or foreground
	int foreground;
	int rv;
	char * mountpoint;
	rv = fuse_parse_cmdline(&main_args, &mountpoint, NULL, &foreground);
	if (rv == -1)
	{
		message(LOG_INFO, FACILITY_ZFSD, "Failed to parse fuse cmdline options.\n");
		exit(EXIT_FAILURE);
	}

	set_mountpoint(mountpoint);
	free(mountpoint);

#ifndef ENABLE_CLI_CONSOLE //cli use console, don't daemonize
	rv = fuse_daemonize(foreground);
	if (rv == -1)
	{
		message(LOG_INFO, FACILITY_ZFSD, "Failed to daemonize zfsd.\n");
		exit(EXIT_FAILURE);
	}
#endif

}

void free_arguments(void)
{
	fuse_opt_free_args(&main_args);
}

