/**
 * Responses send from mbroker to the various clients
 */

#include "response.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Initialize a response sent from mbroker to a manager client.
 * Must provide the OP_CODE of the response aswell as the return code and error
 * message (empty string if no error i.e ret_code = 0)
 */
int manager_response_init(manager_response_t *resp, uint8_t op_code,
                          int32_t ret_code, char *message) {
    memset(resp, 0, sizeof(*resp));

    resp->op_code = op_code;
    resp->ret_code = ret_code;

    if (message != NULL) {
        if (strcpy(resp->error_message, message) == NULL) {
            return -1;
        }
    }

    return 0;
}

int manager_response_set_error_msg(manager_response_t *resp, char *msg) {
    if (strcpy(resp->error_message, msg) == NULL) {
        return -1;
    }

    return 0;
}

/**
 * Send a response to the designated manager at FIFO's opened with given fd.
 * Returns 0 if successful and negative values upon failure
 *
 */
int manager_response_send(int fd, manager_response_t *resp) {
    ssize_t ret = write(fd, resp, sizeof(*resp));

    if (ret == -1) {
        if (errno == EPIPE)
            return -1;
    } else if (ret != sizeof(*resp)) {
        return -2;
    }

    return 0;
}

/**
 * Initializes a response meant to be sent from mbroker to a subscriber client.
 */
int subscriber_response_init(subscriber_response_t *resp, char *message) {
    memset(resp, 0, sizeof(*resp));
    resp->op_code = SUBSCRIBER_OP_CODE;

    if (strcpy(resp->message, message) == NULL) {
        return -1;
    }

    return 0;
}

/**
 * Send a response to a subscriber client, uses publisher_request_send because
 * both have the same policy
 */
int subscriber_response_send(int fd, subscriber_response_t *resp) {
    return publisher_request_send(fd, resp);
}