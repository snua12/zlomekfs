#include "system.h"
#include <stdio.h>
#include "control.h"
#include "zfsd_state.h"

/*dummy zfsd_get_state implementation*/
zfsd_state_e zfsd_get_state(void)
{
  return ZFSD_STATE_STARTING;
}

int main(ATTRIBUTE_UNUSED int argc, ATTRIBUTE_UNUSED char * argv[])
{
  initialize_control_c();
  printf("Initialized\n");
  cleanup_control_c();
  return 0;
}

