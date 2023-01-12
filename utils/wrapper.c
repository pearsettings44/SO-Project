#include <pthread.h>
#include <stdio.h>

// wrapper function to lock a mutex
int mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        fprintf(stderr, "ERR couldn't unlock mutex\n");
        return -1;
    }
    return 0;
}

// wrapper function to unlock a mutex
int mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "ERR couldn't unlock mutex\n");
        return -1;
    }
    return 0;
}

// wrapper function to wait on condition
int cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex) {
    if (pthread_cond_wait(condition, mutex) != 0) {
        fprintf(stderr, "ERR couldn't wait on condition\n");
        return -1;
    }
    return 0;
}

// wrapper function to signal condition
int cond_broadcast(pthread_cond_t *condition) {
    if (pthread_cond_broadcast(condition) != 0) {
        fprintf(stderr, "ERR couldn't broadcast condition\n");
        return -1;
    }
    return 0;
}