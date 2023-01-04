#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

void rwl_init(pthread_rwlock_t *rwlock);
void rwl_destroy(pthread_rwlock_t *rwlock);
void rwl_rdlock(pthread_rwlock_t *rwlock);
void rwl_wrlock(pthread_rwlock_t *rwlock);
void rwl_unlock(pthread_rwlock_t *rwlock);

void mutex_init(pthread_mutex_t *lock);
void mutex_destroy(pthread_mutex_t *lock);
void mutex_lock(pthread_mutex_t *lock);
void mutex_unlock(pthread_mutex_t *lock);

#endif