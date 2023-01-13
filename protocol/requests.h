#ifndef REQUESTS_H
#define REQUESTS_H

#include <stdint.h>

/**
 * Protocol OP-CODES
 */
#define PUB_REGISTER_OP 1
#define SUB_REGISTER_OP 2
#define CREATE_BOX_OP 3
#define DELETE_BOX_OP 5
#define LIST_BOX_OP 7
#define PUBLISH_OP_CODE 9
#define SUBSCRIBER_OP_CODE 10

#define MAX_PIPE_PATHNAME 256
#define MAX_BOX_NAME 32
#define MESSAGE_LENGTH 1024

struct __attribute__((__packed__)) registration_request_t {
    uint8_t op_code;
    char pipe_name[MAX_PIPE_PATHNAME];
    char box_name[MAX_BOX_NAME];
};

struct __attribute__((__packed__)) publisher_request_t {
    uint8_t op_code;
    char message[MESSAGE_LENGTH];
};

typedef struct registration_request_t registration_request_t;
typedef struct publisher_request_t publisher_request_t;

int registration_request_init(registration_request_t *req, uint8_t op_code,
                              char *pipe_name, char *box_name);
int registration_request_send(int fd, registration_request_t *req);
int registration_request_mkfifo(registration_request_t *req);

int publisher_request_init(publisher_request_t *req, char *message);
int publisher_request_send(int fd, publisher_request_t *req);

#endif