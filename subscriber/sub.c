#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "logging.h"
#include "requests.h"
#include "response.h"

static int interrupt_var;

/**
 * Simply signal the main loop that the event has occurred. As an alternative
 * static sessions structures could be used
 */
static void sigint_handler(int sig) {
    if (sig == SIGINT) {
        interrupt_var = 1;
    }
}

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: sub <register_pipe_name> <pipe_name> \
                <box_name>\n");
        exit(EXIT_FAILURE);
    }

    // assign SIGTERM handler
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr, SIGNAL_ERR_MSG, "SIGINT");
        exit(EXIT_FAILURE);
    }

    int sub_fd = connect_to_mbroker(argv[1], argv[2], argv[3], SUB_REGISTER_OP,
                                    O_RDONLY);
    char *pipe_name = argv[2];

    // registration request to be sent to mbroker
    // registration_request_t req;

    // initialize registration request
    // if (registration_request_init(&req, SUB_REGISTER_OP, argv[2], argv[3]) !=
    //     0) {
    //     fprintf(stderr, REQUEST_INIT_ERR_MSG, SUB_REGISTER_OP);
    //     exit(EXIT_FAILURE);
    // }

    // // create FIFO used to communicate with mbroker
    // if (registration_request_mkfifo(&req) != 0) {
    //     fprintf(stderr, PIPE_CREATE_ERR_MSG, req.pipe_name);
    //     exit(EXIT_FAILURE);
    // }

    // // open writting pipe end
    // int mbroker_fd = open(argv[1], O_WRONLY);
    // // couldn't open pipe
    // if (mbroker_fd == -1) {
    //     fprintf(stderr, PIPE_OPEN_ERR_MSG, argv[1]);
    //     exit_failure(-1, req.pipe_name);
    // }

    // // make request to mbroker
    // if (registration_request_send(mbroker_fd, &req) != 0) {
    //     fprintf(stderr, REQUEST_SEND_ERR_MSG, req.op_code);
    //     exit_failure(mbroker_fd, req.pipe_name);
    // }

    // close(mbroker_fd);

    // // open communication FIFO
    // int sub_fd = open(req.pipe_name, O_RDONLY);
    // // couldn't open pipe
    // if (sub_fd == -1) {
    //     fprintf(stderr, PIPE_OPEN_ERR_MSG, req.pipe_name);
    //     exit_failure(-1, req.pipe_name);
    // }

    subscriber_response_t sub_resp;

    unsigned int nr_messages = 0;
    while (1) {
        // read response sent from mbroker
        ssize_t ret = read(sub_fd, &sub_resp, sizeof(sub_resp));
        /**
         * If failed to read. Checks for interrupt var because it will take
         * a cycle for it to be detected
         */
        if (ret == 0) {
            fprintf(stderr, INVALID_OP_ERR_MSG);
            break;
        } else if (ret != sizeof(sub_resp) && interrupt_var != 1) {
            fprintf(stderr, PIPE_PARTIAL_RW_ERR_MSG, pipe_name);
            exit_failure(sub_fd, pipe_name);
        }
        // if SIGTERM was received
        if (interrupt_var == 1) {
            fprintf(stdout, SIGINT_MSG);
            break;
        }
        // print message received from mbroker
        nr_messages++;
        fprintf(stdout, "%s\n", sub_resp.message);
    }

    // print number of read messages
    fprintf(stdout, "%d\n", nr_messages);

    // cleanup
    close(sub_fd);
    if (unlink(pipe_name) != 0) {
        fprintf(stderr, PIPE_DELETE_ERR_MSG, pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
