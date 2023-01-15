/**
 * Requests made by the various clients to mbroker
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "requests.h"

/**
 * Initializes a registration request to be made to mbroker from the clients.
 * Takes the OP_CODE of the request, the PIPE_NAME of the client and the
 * BOX_NAME solicited to mbroker (NULL if none) Returns 0 on success and -1 upon
 * failure
 */
int registration_request_init(registration_request_t *req, uint8_t op_code,
                              char *pipe_name, char *box_name) {
    memset(req, 0, sizeof(*req));

    req->op_code = op_code;

    if (strcpy(req->pipe_name, pipe_name) == NULL) {
        return -1;
    }

    if (box_name != NULL) {
        if (strcpy(req->box_name, box_name) == NULL) {
            return -1;
        }
        // truncate if necessary
        req->box_name[BOX_NAME_LENGTH - 1] = 0;
    }

    // truncate if necessary
    req->pipe_name[PIPE_PATHNAME_LENGTH - 1] = 0;

    return 0;
}

/**
 * Requests a registration of a client using fd of mbroker's FIFO.
 *
 * Returns 0 upon success and negative values upon failure
 *
 * Return error values:
 *  -1 if failed to write to FIFO
 *  -2 if partial write
 */
int registration_request_send(int fd, registration_request_t *req) {
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
int registration_request_mkfifo(registration_request_t *req) {
    if (mkfifo(req->pipe_name, S_IRUSR | S_IWUSR | S_IRGRP) == -1 &&
        errno != EEXIST) {
        return -1;
    }
    return 0;
}

/**
 * Initializes a registration request to be made to mbroker from the clients.
 */
int publisher_request_init(publisher_request_t *req, char *message) {
    req->op_code = PUBLISH_OP_CODE;

    if (strcpy(req->message, message) == NULL) {
        return -1;
    }

    return 0;
}

/**
 * Sends a publisher request to mbroker though FIFO opened with FD.
 * Returns 0 on success and negative values on failure
 *
 * Error returns:
 *  -1 if EPIPE error (closed reading END pipe)
 *  -2 if other failures writting to FIFO
 *  -3 if partial write
 */
int publisher_request_send(int fd, publisher_request_t *req) {
    ssize_t ret = write(fd, req, sizeof(*req));

    if (ret == -1) {
        if (errno == EPIPE) {
            return -1;
        } else {
            return -2;
        }
    } else if (ret != sizeof(*req)) {
        return -3;
    }

    return 0;
}
