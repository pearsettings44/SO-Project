#ifndef BOX_H
#define BOX_H

#include "response.h"
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

typedef enum box_allocation_state_t { USED, NOT_USED } box_allocation_state_t;
struct box_t {
    char name[BOX_NAME_LENGTH];
    uint64_t n_publishers;
    uint64_t n_subscribers;
    size_t size;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
    box_allocation_state_t alloc_state;
};

typedef struct box_t box_t;
int create_box(manager_response_t *resp, char *name);
int delete_box(manager_response_t *resp, char *name);
int box_initialize(box_t *box, char *name);

ssize_t write_message(int fd, char *msg);
ssize_t read_message(int fd, char *msg);

#endif