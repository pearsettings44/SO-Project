#ifndef __WRAPPER_H__
#define __WRAPPER_H__

#include <pthread.h>

int mutex_init(pthread_mutex_t *mutex);
int mutex_destroy(pthread_mutex_t *mutex);
int mutex_lock(pthread_mutex_t *mutex);
int mutex_unlock(pthread_mutex_t *mutex);

int cond_init(pthread_cond_t *cond);
int cond_destroy(pthread_cond_t *cond);
int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int cond_broadcast(pthread_cond_t *cond);

#endif