/*! \file
    \brief Implementation of control client functions (high level logger-control interface)
    \see control.h
*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Syplog.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#define _GNU_SOURCE
#include <unistd.h>
#undef _GNU_SOURCE

#include "control.h"
#include "errno.h"

/** resolve address to struct in_addr
 * @param addr IP address only so far
 * @param target valid pointer to struct in_addr where to store resolved address
 * @return ERR_BAD_PARAMS, NOERR
*/
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
