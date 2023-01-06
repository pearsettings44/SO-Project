#ifndef REQUESTS_H
#define REQUESTS_H

#include <stdint.h>

// Protocol OP-CODES 
#define PUB_REGISTER_OP 1
#define SUB_REGISTER_OP 2
#define MAN_REGISTER_OP 3 

#define MAX_PIPE_PATHNAME 256
#define MAX_BOX_NAME 32

struct request_t {
    uint8_t op_code;
    char pipe_name[MAX_PIPE_PATHNAME];
    char box_name[MAX_BOX_NAME];
};

typedef struct request_t request_t;

int request_init(request_t *req, char **args);
int request_register(int fd, request_t *req, uint8_t op_code);
int request_mkfifo(request_t *req);

#endif