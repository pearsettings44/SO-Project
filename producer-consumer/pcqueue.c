#include "producer-consumer.h"
#include "requests.h"
#include "utils.h"
#include <pthread.h>
#include <stdlib.h>

int pcq_create(pc_queue_t *queue, size_t capacity) {
    queue->pcq_buffer = malloc(sizeof(registration_request_t *) * capacity);
    if (queue->pcq_buffer == NULL) {
        return -1;
    }

    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    mutex_init(&queue->pcq_current_size_lock);
    mutex_init(&queue->pcq_head_lock);
    mutex_init(&queue->pcq_tail_lock);
    mutex_init(&queue->pcq_pusher_condvar_lock);
    mutex_init(&queue->pcq_popper_condvar_lock);
    cond_init(&queue->pcq_pusher_condvar);
    cond_init(&queue->pcq_popper_condvar);

    return 0;
}

int pcq_destroy(pc_queue_t *queue) {
    free(queue->pcq_buffer);

    mutex_destroy(&queue->pcq_current_size_lock);
    mutex_destroy(&queue->pcq_head_lock);
    mutex_destroy(&queue->pcq_tail_lock);
    mutex_destroy(&queue->pcq_pusher_condvar_lock);
    mutex_destroy(&queue->pcq_popper_condvar_lock);
    cond_destroy(&queue->pcq_pusher_condvar);
    cond_destroy(&queue->pcq_popper_condvar);

    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem) {
    mutex_lock(&queue->pcq_pusher_condvar_lock);
    mutex_lock(&queue->pcq_current_size_lock);
    while (queue->pcq_current_size == queue->pcq_capacity) {
        mutex_unlock(&queue->pcq_current_size_lock);
        cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
        mutex_lock(&queue->pcq_current_size_lock);
    }
    mutex_lock(&queue->pcq_head_lock);
    // add element to queue
    queue->pcq_current_size++;
    queue->pcq_buffer[queue->pcq_head] = elem;
    // update head position
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;
    mutex_unlock(&queue->pcq_head_lock);
    mutex_unlock(&queue->pcq_current_size_lock);
    mutex_unlock(&queue->pcq_pusher_condvar_lock);
    // signal poppers that there is a new request on buffer
    cond_broadcast(&queue->pcq_popper_condvar);

    return 0;
}

void *pcq_dequeue(pc_queue_t *queue) {
    mutex_lock(&queue->pcq_popper_condvar_lock);
    mutex_lock(&queue->pcq_current_size_lock);
    while (queue->pcq_current_size == 0) {
        mutex_unlock(&queue->pcq_current_size_lock);
        cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock);
        mutex_lock(&queue->pcq_current_size_lock);
    }
    mutex_lock(&queue->pcq_tail_lock);
    queue->pcq_current_size--;
    // pop element
    void *elem = queue->pcq_buffer[queue->pcq_tail];
    // update new tail position
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    mutex_unlock(&queue->pcq_tail_lock);
    mutex_unlock(&queue->pcq_current_size_lock);
    mutex_unlock(&queue->pcq_popper_condvar_lock);
    // signal pushers that a free position has been created
    cond_broadcast(&queue->pcq_pusher_condvar);

    return elem;
}
