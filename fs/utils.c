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

/**
 * Wrapper for pthread_rwlock_destroy
 */
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
/**
 * Wrapper for pthread_mutex_init with default arguments
 */
void mutex_init(pthread_mutex_t *lock) {
    if (pthread_mutex_init(lock, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }
}
/**
 * Wrapper for pthread_mutex_destroy
 */
void mutex_destroy(pthread_mutex_t *lock) {
    if (pthread_mutex_destroy(lock) != 0) {
        perror("Failed to destroy mutex");
        exit(EXIT_FAILURE);
    }
}

/**
 * Wrapper for pthread_mutex_lock
 */
void mutex_lock(pthread_mutex_t *lock) {
    if (pthread_mutex_lock(lock) != 0) {
        perror("Failed to lock mutex");
        exit(EXIT_FAILURE);
    }
}

/**
 * Wrapper for pthread_mutex_unlock
 */
void mutex_unlock(pthread_mutex_t *lock) {
    if (pthread_mutex_unlock(lock) != 0) {
        perror("Failed to unlock mutex");
        exit(EXIT_FAILURE);
    }
}