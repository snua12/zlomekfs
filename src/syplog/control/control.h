#ifndef CONTROL_H
#define	CONTROL_H

#include "control-protocol.h"
#include "log-constants.h"

syp_error send_uint32_by_function (uint32_t data, 
  syp_error (*function)(int, uint32_t, const struct sockaddr *, socklen_t), 
  const char * ip, uint16_t port);

#define set_level_udp(level,addr,port)	send_uint32_by_function(level,set_level_sendto,addr,port)

#define set_facility_udp(facility,addr,port) \
	send_uint32_by_function(facility,set_facility_sendto,addr,port)

#define reset_facility_udp(facility,addr,port) \
	send_uint32_by_function(facility,reset_facility_sendto,addr,port)

#endif	/* CONTROL_H */
