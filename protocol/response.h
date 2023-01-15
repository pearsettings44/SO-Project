#ifndef RESPONSE_H
#define RESPONSE_H

#include "requests.h"
#include <stdint.h>

/**
 * Configuration macros
 */
#define ERR_MESSAGE_LENGTH 1024

/**
 * Protocol OP-CODES
 */
#define LIST_MANAGER_OP 8

/**
 * Error messages
 */
#define RESPONSE_INIT_ERR_MSG "ERROR: Failed initializing %d op_code response\n"
#define RESPONSE_SEND_ERR_MSG                                                  \
    "ERROR: Failed sending %d op_code response to client\n"

/**
 * Error messages passed in broker response to manager
 */
#define RESP_ERR_DUPLICATE_BOX "Box already exists\n"
#define RESP_ERR_INIT_BOX "Couldn't initialize box %s\n"
#define RESP_ERR_CREATE_BOX "Couldn't create box\n"
#define RESP_ERR_UNKNOWN_BOX "Box doesn't exist\n"
#define RESP_ERR_DELETE_BOX "Couldn't delete box\n"

struct __attribute__((__packed__)) manager_response_t {
    uint8_t op_code;
    int32_t ret_code;
    char error_message[ERR_MESSAGE_LENGTH];
};

struct __attribute__((__packed__)) list_manager_response_t {
    uint8_t op_code;
    uint8_t last_flag;
    char box_name[BOX_NAME_LENGTH];
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