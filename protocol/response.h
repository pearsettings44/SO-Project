#ifndef RESPONSE_H
#define RESPONSE_H

#include "requests.h"
#include <stdint.h>

#define ERR_MESSAGE_LENGTH 1024
#define LIST_MANAGER_OP 8

struct __attribute__((__packed__)) manager_response_t {
    uint8_t op_code;
    int32_t ret_code;
    char error_message[ERR_MESSAGE_LENGTH];
};

struct __attribute__((__packed__)) list_manager_response_t {
    uint8_t op_code;
    uint8_t last_flag;
    char box_name[MAX_BOX_NAME];
    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
};

typedef struct manager_response_t manager_response_t;
// publisher and subscriber's request have the same policy
typedef struct publisher_request_t subscriber_response_t;

typedef struct list_manager_response_t list_manager_response_t;

int manager_response_init(manager_response_t *, uint8_t op_code,
                          int32_t ret_code, char *msg);
int manager_response_send(int fd, manager_response_t *resp);
int manager_response_set_error_msg(manager_response_t *, char *msg);

int list_manager_response_init(list_manager_response_t *resp, uint8_t last_flag,
                               char *box_name, uint64_t box_size,
                               uint64_t n_publishers, uint64_t n_subscribers);
int list_manager_response_send(int fd, list_manager_response_t *resp);

int subscriber_response_init(subscriber_response_t *resp, char *message);
int subscriber_response_send(int fd, subscriber_response_t *resp);

#endif