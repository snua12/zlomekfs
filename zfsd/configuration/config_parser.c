#include "system.h"
#include "log.h"
#include "memory.h"
#include "config_parser.h"

/* ! Split the line by ':', trim the resulting parts, fill up to N parts to
   PARTS and return the total number of parts.  */

int split_and_trim(char *line, int n, string * parts)
{
	int i;
	char *start, *colon;

	i = 0;
	while (1)
	{
		/* Skip white spaces.  */
		while (*line == ' ' || *line == '\t')
			line++;

		/* Remember the beginning of a part. */
		start = line;
		if (i < n)
			parts[i].str = start;

		/* Find the end of a part.  */
		while (*line != 0 && *line != '\n' && *line != ':')
			line++;
		colon = line;

		if (i < n)
		{
			if (line > start)
			{
				/* Delete white spaces at the end of a part.  */
				line--;
				while (line >= start && (*line == ' ' || *line == '\t'))
				{
					*line = 0;
					line--;
				}
				line++;
			}
			parts[i].len = line - start;
		}

		i++;

		if (*colon == ':')
		{
			*colon = 0;
			line = colon + 1;
		}
		else
		{
			/* We are at the end of line.  */
			*colon = 0;
			break;
		}
	}

	return i;
}

