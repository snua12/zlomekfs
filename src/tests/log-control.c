#define _GNU_SOURCE

#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#undef _GNU_SOURCE

#include "control.h"

void print_help()
{
  printf ("program [args]\n"
    "--action=act -a act  action\n"
    "\t%d ping, %d set level, %d set facility %d reset facility\n"
    "--data=dat -d dat data to send\n"
    "--help -h this help\n\n",
    MESSAGE_PING, MESSAGE_SET_LEVEL, MESSAGE_SET_FACILITY, MESSAGE_RESET_FACILITY);
}

int main (int argc, char ** argv)
{
  message_type action = MESSAGE_PING;
  uint32_t data = 0;
  syp_error ret_code = NOERR;


  // table of known options
  const struct option option_table[] = 
  {
    {"action",	1, NULL,			'a'},
    {"data",	1, NULL,			'd'},
    {"help",	1, NULL,			'h'},

    {NULL, 0, NULL, 0}
  }; 

  int opt;


  // we need to "init" structures of getopt library
  optind=0;
	
  while ((opt = getopt_long(argc, (char**)argv, "", option_table, NULL)) != -1)
    switch(opt){
      case 'a': // medium type
        action = atoi (optarg);
        break;
      case 'd': // formater type
        data = atoi (optarg);
        break;
      case 'h':
      case '?':
      default:
        print_help();
        exit (0);
        break;
    }

  switch (action)
  {
    case MESSAGE_SET_LEVEL:
      ret_code = set_level_udp(data,NULL,0);
      break;
    case MESSAGE_SET_FACILITY:
      ret_code = set_facility_udp(data,NULL,0);
      break;
    case MESSAGE_RESET_FACILITY:
      ret_code = reset_facility_udp(data,NULL,0);
      break;
    case MESSAGE_PING:
    default:
      printf ("unsupported action %d\n", action);
      exit (1);
  }
  
  if (ret_code == NOERR)
    printf ("action successfully send\n");
  else
    printf ("action send error: %d - %s\n", ret_code, syp_error_to_string (ret_code));

  exit (0);

}
