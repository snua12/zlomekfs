/**
 *  \file barrier.h
 *  \brief Pthread barrier implementation.
 *  \author Ales Snuparek (based on http://www.howforge.com/implementing-barrier-in-pthreads)
 *
 */

#ifndef BARRIER_H
#define BARRIER_H
#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct 
{
	int needed;
	int called;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
}
barrier_t;

int barrier_init(barrier_t *barrier, int needed);
int barrier_destroy(barrier_t *barrier);
int barrier_wait(barrier_t *barrier);

#ifdef __cplusplus
}
#endif

#endif

