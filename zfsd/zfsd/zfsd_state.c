#include "system.h"
#include "zfsd_state.h"

static zfsd_state_e zfsd_state = ZFSD_STATE_STARTING;

void zfsd_set_state(zfsd_state_e state)
{
  zfsd_state = state;
}

zfsd_state_e zfsd_get_state(void)
{
  return zfsd_state;
}
