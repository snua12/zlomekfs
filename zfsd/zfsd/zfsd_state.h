#ifndef ZFSD_STATE_H
#define ZFSD_STATE_H

typedef enum {
  ZFSD_STATE_STARTING = 0,
  ZFSD_STATE_RUNNING = 1,
  ZFSD_STATE_TERMINATING = 10,
  ZFSD_STATE_UNKNOWN = 11
} zfsd_state_e;


// updates zfs daemon state
void zfsd_set_state(zfsd_state_e state);

// get zfs daemon state
zfsd_state_e zfsd_get_state(void);

#endif
