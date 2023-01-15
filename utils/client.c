#include "client.h"
#include "requests.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * Connect client to mbroker. Returns FD that allows for communication.
 *
 * A FIFO with PIPE_NAME will be created and opened with COMM_FLAGS for
 * communication. A registration request with OP_CODE code will be
 * made to mbroker using FIFO BROKER_PIPE to begin communication.
 *
 * Upon failure the proccess will terminate
 */
int connect_to_mbroker(char *broker_pipe, char *pipe_name, char *box_name,
                       uint8_t op_code, int comm_flags) {
    // registration request to be sent to mbroker
    registration_request_t req;

    // initialize registration request
    if (registration_request_init(&req, op_code, pipe_name, box_name) != 0) {
        fprintf(stderr, REQUEST_INIT_ERR_MSG, op_code);
        exit(EXIT_FAILURE);
    }

    // create FIFO used for communication with  mbroker
    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, PIPE_CREATE_ERR_MSG, req.pipe_name);
        exit(EXIT_FAILURE);
    }

    // open mbroker registration pipe
    int mbroker_fd = open(broker_pipe, O_WRONLY);
    // couldn't open pipe
    if (mbroker_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, broker_pipe);
        exit_failure(-1, req.pipe_name);
    }

    // make request to mbroker
    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, REQUEST_SEND_ERR_MSG, req.op_code);
        exit_failure(mbroker_fd, req.pipe_name);
    }

    // close mbroker registration pipe
    close(mbroker_fd);

    // open communication FIFO
    int fd = open(req.pipe_name, comm_flags);
    // couldn't open pipe
    if (fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, req.pipe_name);
        exit_failure(-1, req.pipe_name);
    }

    return fd;
}
/**
 * End client session when it fails to connect to mbroker. Closes currently
 * opened connection in PUB_FD and removes FIFO PIPE_NAME used for communication
 * with mbroker.
 */
void exit_failure(int pub_fd, char *pipe_name) {
    if (pub_fd != -1) {
        close(pub_fd);
    }
    if (unlink(pipe_name) != 0) {
        fprintf(stderr, PIPE_DELETE_ERR_MSG, pipe_name);
    }
    exit(EXIT_FAILURE);
}