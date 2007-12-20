
#define _GNU_SOURCE
#include <unistd.h>
#undef _GNU_SOURCE

#include "control.h"
#include "errno.h"

syp_error resolve_host (const char * addr, struct in_addr * target)
{
 /* TODO: use
  int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res);
 */
 if (inet_aton(addr, target) == 0)
   return ERR_BAD_PARAMS;
 
 return NOERR;
 
}


syp_error send_uint32_by_function (uint32_t data, 
  syp_error (*function)(int, uint32_t, const struct sockaddr *, socklen_t), 
  const char * ip, uint16_t port)
{
  struct sockaddr_in addr;
  int sock = -1;
  syp_error ret_code = NOERR;
  
  if (ip ==  NULL)
    ip = DEFAULT_COMMUNICATION_ADDRESS;
  if (port == 0)
    port = DEFAULT_COMMUNICATION_PORT;
    
  // init socket
  sock = socket(AF_INET,SOCK_DGRAM,0);
  if (sock == -1)
  {
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  
  ret_code = resolve_host (ip, &(addr.sin_addr));
  if (ret_code != NOERR)
    goto FINISHING;
  
  ret_code = function (sock, data, (struct sockaddr *)&addr, sizeof (struct sockaddr_in));
  
FINISHING:
  if (sock >= 0)
    close (sock);
    
  return ret_code;
}
