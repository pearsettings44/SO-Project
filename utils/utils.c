#include "logging.h"
#include <pthread.h>
#include <stdio.h>

/**
 * Initializes given mutex. Exit process if operation fails
 */
int mutex_init(pthread_mutex_t *mutex) {
    if (pthread_mutex_init(mutex, NULL) != 0) {
        PANIC("FATAL: Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Destroys given mutex. Exit process if operation fails
 */
int mutex_destroy(pthread_mutex_t *mutex) {
    if (pthread_mutex_destroy(mutex) != 0) {
        PANIC("FATAL: Failed to destroy mutex");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Locks given mutex. Exit process if operation fails
 */
int mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        PANIC("FATAL: Failed to lock mutex");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Unlocks given mutex. Exit process if operation fails
 */
int mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        PANIC("FATAL: Failed to unlock mutex");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Initializes given conditional variable. Exit process if operation fails
 */
int cond_init(pthread_cond_t *conditional) {
    if (pthread_cond_init(conditional, NULL) != 0) {
        PANIC("FATAL: Failed to initialize conditional variable");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Destroys given conditional variable. Exit process if operation fails
 */
int cond_destroy(pthread_cond_t *conditional) {
    if (pthread_cond_destroy(conditional) != 0) {
        PANIC("FATAL: Failed to destroy conditional variable");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Waits on given conditional variable. Exit process if operation fails
 */
int cond_wait(pthread_cond_t *conditional, pthread_mutex_t *mutex) {
    if (pthread_cond_wait(conditional, mutex) != 0) {
        PANIC("FATAL: Failed to wait on conditional variable");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Signals given conditional variable. Exit process if operation fails
 */
int cond_signal(pthread_cond_t *conditional) {
    if (pthread_cond_signal(conditional) != 0) {
        PANIC("FATAL: Failed to signal conditional variable");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Broadcasts on given conditional variable. Exit process if operation fails
 */
int cond_broadcast(pthread_cond_t *conditional) {
    if (pthread_cond_broadcast(conditional) != 0) {
        PANIC("FATAL: Failed to broadcast on conditional variable");
        exit(EXIT_FAILURE);
    }
    return 0;
}