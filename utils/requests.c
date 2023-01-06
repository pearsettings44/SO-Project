#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "requests.h"

/**
 * Initializes a registration request to be made to mbroker from the clients.
 */
int request_init(request_t *req, char **args) {
    memset(req, 0, sizeof(*req));

    if (strcpy(req->pipe_name, args[0]) == NULL) {
        return -1;
    }
    if (strcpy(req->box_name, args[1]) == NULL) {
        return -2;
    }

    return 0;
}

/**
 * Requests a registration of a client using fd of mbroker's FIFO.
 * 
 * Returns 0 upon success and negative values upon failure
 * 
 * Error returns:
 *  -1 if failed to write to FIFO
 *  -2 if partial write
 */
int request_register(int fd, request_t *req, uint8_t op_code) {
    req->op_code = op_code;
    ssize_t ret = write(fd, req, sizeof(*req));

    if (ret == -1) {
        return -1;
    } else if (ret != sizeof(*req)) {
        return -2;
    }

    return 0;
}

/**
 * Create a FIFO designated by a registration request to mbroker.
 * 
 * Returns 0 upon success and -1 if failed.
 */
int request_mkfifo(request_t *req) {
    if (mkfifo(req->pipe_name, S_IRUSR | S_IWUSR | S_IRGRP) == -1 
                    && errno != EEXIST) {
        return -1;
    }
    return 0;
}
