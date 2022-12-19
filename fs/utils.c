/**
 * Wrapper functions for pthread functions. Handles and\or exits on errors
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


/**
 * Wrapper for pthread_rwlock_init with default arguments
 */
void rwl_init(pthread_rwlock_t *rwlock) {
    if (pthread_rwlock_init(rwlock, NULL) != 0) {
        perror("Failed initializing rwlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_destroy(pthread_rwlock_t *rwlock) {
    if (pthread_rwlock_destroy(rwlock) != 0) {
        perror("Failed destroying rwlock");
        exit(EXIT_FAILURE);
    }
}

/**
 * Wrapper for pthread_rwlock_rdlock
 */
void rwl_rdlock(pthread_rwlock_t *rwlock) {
    if (pthread_rwlock_rdlock(rwlock) != 0) {
        perror("Failed to lock rwlock for reading");
        exit(EXIT_FAILURE);
    }
}

/**
 * Wrapper for pthread_rwlock_wrlock
 */
void rwl_wrlock(pthread_rwlock_t *rwlock) {
    if (pthread_rwlock_wrlock(rwlock) != 0) {
        perror("Failed to lock rwlock for writing");
        exit(EXIT_FAILURE);
    }
}

/**
 * Wrapper for pthread_rwlock_unlock
 */
void rwl_unlock(pthread_rwlock_t *rwlock) {
    if (pthread_rwlock_unlock(rwlock) != 0) {
        perror("Failed to unlock rwlock");
        exit(EXIT_FAILURE);
    }
}