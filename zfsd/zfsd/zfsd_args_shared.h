/**
 *  \file zfsd_args_shared.h
 * 
 *  \brief Implements shared functions for zfs_args_*.[ch] functions
 *  \author Ales Snuparek
 */

#ifndef ZFSD_ARGS_SHARED_H
#define ZFSD_ARGS_SHARED_H

#ifdef __cplusplus
extern "C"
{
#endif

/* 
 *! Display the usage and arguments.  
 */

void usage(void);


/* 
 *! Display the version, exit the program with exit code EXITCODE.  
 */

void version(int exitcode);



#ifdef __cplusplus
}
#endif

#endif
