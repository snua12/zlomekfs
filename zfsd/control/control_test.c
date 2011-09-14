#include "system.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "control.h"
#include "zfsd_state.h"
#include "syplog.h"

syp_error control_wrap_set_log_level (logger glogger, log_level_t level)
{
  return 0;
}

syp_error control_wrap_set_facility (logger glogger, facility_t facility)
{
  return 0;
}

syp_error control_wrap_reset_facility (logger glogger, facility_t facility)
{
  return 0;
}

/*dummy zfsd_get_state implementation*/
zfsd_state_e zfsd_get_state(void)
{
  return ZFSD_STATE_STARTING;
}

volatile static bool run = true;

static void
sighandler(ATTRIBUTE_UNUSED int signum)
{
  run = false;
}

void
init_sighandler(void)
{
  struct sigaction sig;
  sig.sa_handler = sighandler;
  sig.sa_flags = SA_RESTART;
  sigaction (SIGHUP, &sig, NULL);
  sigaction (SIGINT, &sig, NULL);
}

int main(ATTRIBUTE_UNUSED int argc, ATTRIBUTE_UNUSED char * argv[])
{
  initialize_control_c();
  init_sighandler();
  printf("Initialized\n");
  while (run == true)
  {
    sleep(1);
  }
  cleanup_control_c();
  printf("Stopped\n");
  return 0;
}

