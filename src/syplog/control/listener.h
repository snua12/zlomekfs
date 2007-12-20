#ifndef	LISTENER_H
#define	LISTENER_H

#include "syp-error.h"
#include "syplog.h"
#include "control-protocol.h"

typedef struct listener_def
{
  logger target;
  communication_type type;
  union {
    uint16_t port;
    char socket_name[FILE_NAME_LEN];
  };
  int socket;
  pthread_t thread_id;
  pthread_mutex_t mutex;
} * listener;

syp_error start_listen_udp (listener controller, logger target, uint16_t port);
syp_error start_listen_unix (listener controller, logger target, const char * socket_name);

syp_error stop_control_listen (listener controller);

#endif	/* LISTENER_H */
