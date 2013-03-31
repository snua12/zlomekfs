/**
 *  \file zfsd_args.h
 * 
 *  \brief Implements command line parsing functions
 *  \author Ales Snuparek
 */

#ifndef ZFS_ARGS_H
#define ZFS_ARGS_H

#include "zfsd_args_shared.h"

#ifdef __cplusplus
extern "C"
{
#endif

//! parse command line arguments
void process_arguments(int argc, char **argv);

//! free memory after command line parse function
void free_arguments(void);

#ifdef __cplusplus
}
#endif

#endif
