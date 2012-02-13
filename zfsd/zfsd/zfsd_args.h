#ifndef ZFS_ARGS_H
#define ZFS_ARGS_H

#include "zfsd_args_shared.h"

#ifdef __cplusplus
extern "C"
{
#endif

void process_arguments(int argc, char **argv);

void free_arguments(void);

#ifdef __cplusplus
}
#endif

#endif
