/* ! \file \brief Main media functions.

   Implementation of medium type independent functions, mainly used as type
   switches, initializers, etc. */


/* Copyright (C) 2007, 2008, 2010 Jiri Zouhar, Rastislav Wartiak

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */


#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include "medium.h"
#include "medium-api.h"
#include "file-medium.h"
#include "shm-medium.h"
#include "print-medium.h"
#include "formatters/formatter-api.h"



/* ! Holds translations from stringified names of media operations to
   medium_operation discriminator */
struct
{
	// / unified name of media operation
	char *name;
	// / medium operation discriminator
	medium_operation kind;
} medium_operation_translation_table[] =
{
	{
	OPERATION_READ_NAME, READ_LOG},
	{
	OPERATION_WRITE_NAME, WRITE_LOG},
	{
	NULL, -1}
};


static medium_operation operation_name_to_enum(const char *operation_name)
{
	// index to reader_translation_table for loop
	int table_index = 0;
#ifdef ENABLE_CHECKING
	if (operation_name == NULL)
		return NO_OPERATION;
#endif

	// try to find operation name in table
	for (table_index = 0;
		 medium_operation_translation_table[table_index].name != NULL;
		 table_index++)
	{
		if (strncmp
			(operation_name,
			 medium_operation_translation_table[table_index].name,
			 OPERATION_NAME_LEN) == 0)
			break;
	}

	// if we end inside of table (not a terminator) we found a operation
	if (table_index >= 0
		&& medium_operation_translation_table[table_index].name != NULL)
	{
		return medium_operation_translation_table[table_index].kind;
	}
	else
		return NO_OPERATION;
}


/* ! Holds translations from stringified names of media to medium_type
   discriminator */
struct
{
	// / unified name of reader type
	char *name;
	// / reader type discriminator
	medium_type type;
} medium_translation_table[] =
{
	/* 
	   {NO_MEDIUM_NAME, NO_MEDIUM}, {SOCKET_MEDIUM_NAME, SOCKET_MEDIUM}, */
	{
	FILE_MEDIUM_NAME, FILE_MEDIUM},
	{
	SHM_MEDIUM_NAME, SHM_MEDIUM},
	{
	PRINT_MEDIUM_NAME, PRINT_MEDIUM},
		/* 
		   {SYSLOG_MEDIUM_NAME, SYSLOG_MEDIUM}, */
	{
	NULL, -1}
};

// / Translates medium type name to medium_type.
/* ! Translates medium type name to medium_type discriminator. @param
   medium_type_name unified type name of medium @return type discriminator
   (medium_type) or NO_MEDIUM */
static medium_type medium_name_to_enum(const char *medium_type_name)
{
	// index to reader_translation_table for loop
	int table_index = 0;
#ifdef ENABLE_CHECKING
	if (medium_type_name == NULL)
		return NO_MEDIUM;
#endif

	// try to find type name in table
	for (table_index = 0;
		 medium_translation_table[table_index].name != NULL; table_index++)
	{
		if (strncmp
			(medium_type_name, medium_translation_table[table_index].name,
			 MEDIUM_NAME_LEN) == 0)
			break;
	}

	// if we end inside of table (not a terminator) we found a medium
	if (table_index >= 0 && medium_translation_table[table_index].name != NULL)
	{
		return medium_translation_table[table_index].type;
	}
	else
		return NO_MEDIUM;
}

void print_media_help(int fd, int tabs)
{
	if (fd == 0)
		fd = 1;

	tabize_print(tabs, fd,
				 "medium defines where and in what manner logs are written.\n");
	tabize_print(tabs, fd, "media options:\n");
	tabs++;

	tabize_print(tabs, fd, "--%s=value, -%c value\ttype of medium\n",
				 PARAM_MEDIUM_TYPE_LONG, PARAM_MEDIUM_TYPE_CHAR);

	tabize_print(tabs, fd,
				 "--%s=value, -%c value\tdefines how to format logs\n",
				 PARAM_MEDIUM_FMT_LONG, PARAM_MEDIUM_FMT_CHAR);
	print_formatters_help(fd, tabs);

	tabize_print(tabs, fd,
				 "--%s=value, -%c value\tdefines if read or write logs\n",
				 PARAM_MEDIUM_OP_LONG, PARAM_MEDIUM_OP_CHAR);

	tabize_print(tabs + 1, fd, "values: %s - read logs, %s - write logs\n",
				 OPERATION_READ_NAME, OPERATION_WRITE_NAME);

	tabize_print(tabs, fd,
				 "--%s=value, -%c value\tdefines size of log (approximatelly)\n",
				 PARAM_MEDIUM_SIZE_LONG, PARAM_MEDIUM_SIZE_CHAR);
	tabize_print(tabs, fd,
				 "(0 means infinite), K, M, G suffixes allowed (1, 1K, 1M, 1G, etc)\n");

	tabize_print(tabs, fd, "\n");
	tabize_print(tabs, fd, "medium specific options:\n");

	print_file_medium_help(fd, tabs);
	print_shm_medium_help(fd, tabs);

}

// table of known options
const struct option option_table[] = {
	{PARAM_MEDIUM_TYPE_LONG, 1, NULL, PARAM_MEDIUM_TYPE_CHAR},
	{PARAM_MEDIUM_FMT_LONG, 1, NULL, PARAM_MEDIUM_FMT_CHAR},
	{PARAM_MEDIUM_OP_LONG, 1, NULL, PARAM_MEDIUM_OP_CHAR},
	{PARAM_MEDIUM_SIZE_LONG, 1, NULL, PARAM_MEDIUM_SIZE_CHAR},

	{NULL, 0, NULL, 0}
};

bool_t is_medium_arg(const char *arg)
{
	if (opt_table_contains((struct option *)option_table, arg))
		return TRUE;

	if (is_file_medium_arg(arg))
		return TRUE;

	if (is_shm_medium_arg(arg))
		return TRUE;

	return FALSE;
}


/** Extracts unit multiplier from string
 *
 * @param size_string string in format number[multiplier]
  for example 1M, 15G, 12, 10K
 * @return 1 for no multiplier, 1024 for K, ...
*/
static int unit_multiplier(char *size_string)
{
	int thousants = 0;
	int ret = 1;
	switch (size_string[strlen(size_string) - 1])
	{
	case 'K':
		thousants = 1;
		break;
	case 'M':
		thousants = 2;
		break;
	case 'G':
		thousants = 3;
		break;
	}
	while (thousants > 0)
	{
		ret *= 1024;
		thousants--;
	}

	return ret;
}

// / Parse type independent parameters of medium to structure
/* ! Parses parameters of medium and initialize settings @param argc number of 
   items in argv @param argv parameters in std format as in "main" (non NULL)
   @param settings pointer to struct, where to store settings given in argv
   @return ERR_BAD_PARAMS on invalid argument in argv, NOERR otherwise */
static syp_error medium_parse_params(int argc, const char **argv,
									 medium settings)
{


	int opt;


#ifdef ENABLE_CHECKING
	if (argv == NULL || settings == NULL)
		return ERR_BAD_PARAMS;
#endif

	// we need to "init" structures of getopt library
	optind = 0;

	while ((opt =
			getopt_long(argc, (char **)argv, "", option_table, NULL)) != -1)
		switch (opt)
		{
		case PARAM_MEDIUM_TYPE_CHAR:	// medium type
			settings->type = medium_name_to_enum(optarg);
			break;
		case PARAM_MEDIUM_FMT_CHAR:	// formatter type
			settings->used_formatter = formatter_for_name(optarg);
			if (settings->used_formatter == NULL)
			{
				return ERR_BAD_PARAMS;
			}
			break;
		case PARAM_MEDIUM_OP_CHAR:
			settings->kind = operation_name_to_enum(optarg);
			break;
		case PARAM_MEDIUM_SIZE_CHAR:
			settings->length = atoll(optarg) * unit_multiplier(optarg);
			break;
		case '?':
		default:
			break;
		}

	return NOERR;
}

// / Initialize medium structure.
syp_error open_medium(struct medium_def * target, int argc, const char **argv)
{
	syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
	if (argv == NULL || target == NULL)
		return ERR_BAD_PARAMS;
#endif

	memset(target, 0, sizeof(struct medium_def));
	target->kind = WRITE_LOG;
	target->used_formatter = DEFAULT_FORMATTER;

	ret_code = medium_parse_params(argc, argv, target);
	if (ret_code != NOERR)
	{
		goto FINISHING;
	}

	switch (target->type)
	{
	case NO_MEDIUM:
		ret_code = open_file_medium(target, 0, NULL);
		if (ret_code != NOERR)
		{
			goto FINISHING;
		}
		break;
	case FILE_MEDIUM:
		ret_code = open_file_medium(target, argc, argv);
		if (ret_code != NOERR)
		{
			goto FINISHING;
		}
		break;
	case SHM_MEDIUM:
		ret_code = open_shm_medium(target, argc, argv);
		if (ret_code != NOERR)
		{
			goto FINISHING;
		}
		break;
	case PRINT_MEDIUM:
		ret_code = open_print_medium(target, 0, NULL);
		if (ret_code != NOERR)
		{
			goto FINISHING;
		}
		break;
	default:
		break;
	}

  FINISHING:
	return ret_code;
}

// / Close medium and free internal pointers
syp_error close_medium(medium target)
{
	syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
	if (target == NULL || target->close_medium == NULL)
	{
		return ERR_BAD_PARAMS;
	}
#endif

	ret_code = target->close_medium(target);
	if (ret_code != NOERR)
	{
		goto FINISHING;
	}

  FINISHING:

	return ret_code;
}

// / do operation on log
syp_error access_medium(medium target, log_struct log)
{
#ifdef ENABLE_CHECKING
	if (target == NULL || target->access_medium == NULL)
	{
		return ERR_BAD_PARAMS;
	}
#endif

	return target->access_medium(target, log);
}
