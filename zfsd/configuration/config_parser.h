#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "system.h"
#include "memory.h"
#include "fh.h"

/* ! Split the line by ':', trim the resulting parts, fill up to N parts to
   PARTS and return the total number of parts.  */

int split_and_trim(char *line, int n, string * parts);


/* ! Read file FH by lines and call function PROCESS for each line.  */

bool
process_file_by_lines(zfs_fh * fh, const char *file_name,
					  int (*process) (char *, const char *, unsigned int,
									  void *), void *data);


/* ! Process one line of configuration file.  Return the length of value.  */
// TODO: create normal parser with flex / bison
int
process_line(const char *file, const int line_num, char *line, char **key,
			 char **value);

#endif
