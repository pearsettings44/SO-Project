/**
 * Responses send from mbroker to the various clients
 */

#include "response.h"
#include <errno.h>
#include "response.h"
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Initializes a response meant to be sent from mbroker to a subscriber client.
 * Sets response message to MESSAGE.
 * Returns 0 if successful and -1 on failure
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
 * Send RESP to a subscriber client through FIFO opened with FD.
 * Returns 0 if successful and negative values upon failure
 *
 * Return error values:
 * -1 if EPIPE received
 * -2 if other failures writting to FIFO
 * -3 if partial write
 */
int subscriber_response_send(int fd, subscriber_response_t *resp) {
    return publisher_request_send(fd, resp);
}

/**
 * Initialize a response sent from mbroker to a manager client.
 * Takes the OP_CODE of the response, the return code RET_CODE and error
 * message MESSAGE (empty string if no error i.e ret_code = 0)
 * Returns 0 if successful and -1 on failure
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

/**
 * Set error message to MSG of response referenced by argument RESP
 * Returns 0 if successful and -1 on failure
 */
int manager_response_set_error_msg(manager_response_t *resp, char *msg) {
    if (strcpy(resp->error_message, msg) == NULL) {
        return -1;
    }

    return 0;
}

/**
 * Send a response to the designated manager at FIFO's opened with given FD.
 * Returns 0 if successful and negative values upon failure
 *
 * Return error codes:
 * -1 if EPIPE received
 * -2 if other failures writting to FIFO
 * -3 if partial write to FIFO
 */
int manager_response_send(int fd, manager_response_t *resp) {
    ssize_t ret = write(fd, resp, sizeof(*resp));

    if (ret == -1) {
        if (errno == EPIPE) {
            return -1;
        } else {
            return -2;
        }
    } else if (ret != sizeof(*resp)) {
        return -3;
    }

    return 0;
}

/**
 * Initialize a response sent from mbroker to a manager client requesting box
 * listing. Constructs response with given BOX_NAME, BOX_SIZE, N_PUBLISHERS and
 * N_SUBSCRIBERS. LAST_FLAG marks response as last response to be sent. Returns
 * 0 if successful and -1 on failure
 */
int list_manager_response_init(list_manager_response_t *resp, uint8_t last_flag,
                               char *box_name, uint64_t box_size,
                               uint64_t n_publishers, uint64_t n_subscribers) {
    memset(resp, 0, sizeof(*resp));

    resp->op_code = LIST_MANAGER_OP;
    resp->last_flag = last_flag;
    resp->box_size = box_size;
    resp->n_publishers = n_publishers;
    resp->n_subscribers = n_subscribers;

    if (box_name != NULL) {
        if (strcpy(resp->box_name, box_name) == NULL) {
            return -1;
        }
    }

    return 0;
}

/**
 * Send response RESP to manager client through FIFO opened with FD
 * Returns 0 if successful and negative values upon failure
 *
 * Return error codes:
 * -1 if EPIPE received
 * -2 if other failures writting to FIFO
 * -3 if partial write
 */
int list_manager_response_send(int fd, list_manager_response_t *resp) {
    ssize_t ret = write(fd, resp, sizeof(*resp));

    if (ret == -1) {
        if (errno == EPIPE) {
            return -1;
        } else {
            return -2;
        }
    } else if (ret != sizeof(*resp)) {
        return -3;
    }

    return 0;
}
