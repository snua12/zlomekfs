#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "system.h"
#include "memory.h"
#include "fh.h"

/* ! Split the line by ':', trim the resulting parts, fill up to N parts to
   PARTS and return the total number of parts.  */

int split_and_trim(char *line, int n, string * parts);

#endif
