#include "system.h"
#include "log.h"
#include "memory.h"
#include "config_parser.h"

/*! Process one line of configuration file.  Return the length of value.  */
//TODO: create normal parser with flex / bison
int
process_line (const char *file, const int line_num, char *line, char **key,
	      char **value)
{
  char *dest;
  enum automata_states {
    STATE_NORMAL,		/* outside quotes and not after backslash */
    STATE_QUOTED,		/* inside quotes and not after backslash  */
    STATE_BACKSLASH,		/* outside quotes and after backslash */
    STATE_QUOTED_BACKSLASH	/* inside quotes and after backslash */
  } state;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  if (*line == 0 || *line == '#' || *line == '\n')
    {
      /* There was no key nor value.  */
      *line = 0;
      *key = line;
      *value = line;
      return 0;
    }

  *key = line;
  /* Skip the key.  */
  while (*line != 0 && *line != '#' && *line != '\n'
	 && *line != ' ' && *line != '\t')
    line++;

  if (*line == 0 || *line == '#' || *line == '\n')
    {
      *line = 0;
      *value = NULL;
      message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Option '%s' has no value\n",
	       file, line_num, *key);
      return 0;
    }
  *line = 0;
  line++;

  /* Skip white spaces.  */
  while (*line == ' ' || *line == '\t')
    line++;

  *value = line;
  dest = line;

  /* Finite automata.  */
  state = STATE_NORMAL;
  while (*line != 0)
    {
      switch (state)
	{
	  case STATE_NORMAL:
	    switch (*line)
	      {
		case '"':
		  line++;
		  state = STATE_QUOTED;
		  break;

		case '\\':
		  line++;
		  state = STATE_BACKSLASH;
		  break;

		case ' ':
		case '\t':
		case '#':
		case '\n':
		  *line = 0;
		  break;

		default:
		  *dest++ = *line++;
		  break;
	      }
	    break;

	  case STATE_QUOTED:
	    switch (*line)
	      {
		case '"':
		  line++;
		  state = STATE_NORMAL;
		  break;

		case '\\':
		  line++;
		  state = STATE_QUOTED_BACKSLASH;
		  break;

		case '\n':
		case 0:
		  *line = 0;
		  break;

		default:
		  *dest++ = *line++;
		  break;
	      }
	    break;

	  case STATE_BACKSLASH:
	    *dest++ = *line++;
	    state = STATE_NORMAL;
	    break;

	  case STATE_QUOTED_BACKSLASH:
	    *dest++ = *line++;
	    state = STATE_QUOTED;
	    break;
	}
    }

  /* If there was '\\' on the end of line, add it to the end of string. */
  if (state == STATE_BACKSLASH || state == STATE_QUOTED_BACKSLASH)
    *dest++ = '\\';
  *dest = 0;

  if (*value == dest)
    {
      message (LOG_WARNING, FACILITY_CONFIG, "%s:%d: Option '%s' has no value\n",
	       file, line_num, *key);
      return 0;
    }
  return dest - *value;
}

/*! Split the line by ':', trim the resulting parts, fill up to N parts
   to PARTS and return the total number of parts.  */

int
split_and_trim (char *line, int n, string *parts)
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

/*! Read file FH by lines and call function PROCESS for each line.  */

bool
process_file_by_lines (zfs_fh *fh, const char *file_name,
		       int (*process) (char *, const char *, unsigned int,
				       void *),
		       void *data)
{
  read_res res;
  char buf[ZFS_MAXDATA];
  unsigned int pos, i, line_num;
  uint32_t end;
  uint64_t offset;
  zfs_cap cap;
  int32_t r;

  r = zfs_open (&cap, fh, O_RDONLY);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: open(): %s\n", file_name, zfs_strerror (r));
      return false;
    }

  line_num = 1;
  pos = 0;
  offset = 0;
  for (;;)
    {
      res.data.buf = buf + pos;
      r = zfs_read (&res, &cap, offset, ZFS_MAXDATA - pos, true);
      if (r != ZFS_OK)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s: read(): %s\n", file_name, zfs_strerror (r));
	  return false;
	}

      if (res.data.len == 0)
	break;

      offset += res.data.len;
      end = pos + res.data.len;
      for (pos = 0, i = 0; pos < end; pos = i + 1)
	{
	  for (i = pos; i < end; i++)
	    if (buf[i] == '\n')
	      {
		buf[i] = 0;
		if ((*process) (buf + pos, file_name, line_num, data) != 0)
		  goto finish;
		line_num++;
		break;
	      }
	  if (i == end)
	    break;
	}

      if (pos == 0 && i == ZFS_MAXDATA)
	{
	  message (LOG_ERROR, FACILITY_CONFIG, "%s:%u: Line too long\n", file_name, line_num);
	  goto out;
	}
      if (pos > 0)
	{
	  memmove (buf, buf + pos, end - pos);
	  pos = end - pos;
	}
      else
	{
	  /* The read block does not contain new line.  */
	  pos = end;
	}
    }

finish:
  r = zfs_close (&cap);
  if (r != ZFS_OK)
    {
      message (LOG_ERROR, FACILITY_CONFIG, "%s: close(): %s\n", file_name, zfs_strerror (r));
      return false;
    }

  return true;

out:
  zfs_close (&cap);
  return false;
}

