#ifndef RESPONSE_H
#define RESPONSE_H

#include "requests.h"
#include <stdint.h>

#define ERROR_MESSAGE_LENGTH 1024

struct manager_response_t {
    uint8_t op_code;
    int32_t ret_code;
    char error_message[ERROR_MESSAGE_LENGTH];
};

typedef struct manager_response_t manager_response_t;
// publisher and subscriber's request have the same policy
typedef struct publisher_request_t subscriber_response_t;

int manager_response_init(manager_response_t *, uint8_t op_code,
                          int32_t ret_code, char *msg);
int manager_response_send(int fd, manager_response_t *resp);
int manager_response_set_error_msg(manager_response_t *, char *msg);

int subscriber_response_init(subscriber_response_t *resp, char *message);
int subscriber_response_send(int fd, subscriber_response_t *resp);

#endif