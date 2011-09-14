#ifndef REREAD_CONFIG_H
#define REREAD_CONFIG_H

#include "system.h"
#include "memory.h"

typedef struct reread_config_request_def *reread_config_request;
/* ! \brief Element of list of requests for config reread.  */
struct reread_config_request_def
{
	/* ! Next element in the chain.  */
	reread_config_request next;

	/* ! Path relative to root of config volume.  */
	string relative_path;

	/* ! Node which the request came from.  */
	uint32_t from_sid;
};

void add_reread_config_request(string * relative_path, uint32_t from_sid);

/* ! Reread configuration file RELATIVE_PATH.  */

bool reread_config_file(string * relative_path);


/* ! Reread local info about volumes. \param path Path where local
   configuration is stored.  */

bool reread_local_volume_info(string * path);

/* ! Get a request to reread config from queue and store the relative path of
   the file to be reread to RELATIVE_PATH and the node ID which the request
   came from to FROM_SID.  */

bool get_reread_config_request(string * relative_path, uint32_t * from_sid);


#endif
