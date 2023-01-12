#ifndef BOX_H
#define BOX_H

#include "response.h"
#include <stdint.h>
#include <unistd.h>

#define BOX_NAME 32

struct box_t {
    char name[BOX_NAME];
    uint64_t n_publishers;
    uint64_t n_subscribers;
    size_t size;
};

typedef struct box_t box_t;
int create_box(manager_response_t *resp, char *name);
int delete_box(manager_response_t *resp, char *name);
int box_initialize(box_t *box, char *name);

ssize_t write_message(int fd, char *msg);
ssize_t read_message(int fd, char *msg);

#endif