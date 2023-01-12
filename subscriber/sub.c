#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
        fprintf(stderr, "ERR Failed to assign SIGINT signal handler\n");
        exit(EXIT_FAILURE);
    }

    // request to be sent to mbroker
    registration_request_t req;

    // initialize registration request
    if (registration_request_init(&req, SUB_REGISTER_OP, argv[2], argv[3]) !=
        0) {
        fprintf(stderr,
                "ERR Failed initializing subscriber request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    // create FIFO used to communicate with mbroker
    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "ERR failed creating FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    // open writting pipe end
    int mbroker_fd = open(argv[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "ERR failed opening FIFO %s\n", argv[1]);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    // make request to mbroker
    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "ERR failed registering subscriber\n");
        close(mbroker_fd);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    // open communication FIFO
    int sub_fd = open(req.pipe_name, O_RDONLY);
    if (sub_fd == -1) {
        fprintf(stderr, "ERR failed opening FIFO %s\n", argv[2]);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s\n", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    subscriber_response_t sub_resp;

    while (1) {
        // read response sent from mbroker
        ssize_t ret = read(sub_fd, &sub_resp, sizeof(sub_resp));
        /**
         * If failed to read. Checks for interrupt var because it will take
         * a cycle for it to be detected
         */
        if (ret != sizeof(sub_resp) && interrupt_var != 1) {
            fprintf(stderr, "ERR couldn't read response from pipe\n");
            break;
        }
        // if SIGTERM was received
        if (interrupt_var == 1) {
            fprintf(stdout, "\nSIGINT received, terminating\n");
            break;
        }
        // print message received from mbroker
        fprintf(stdout, "%s\n", sub_resp.message);
    }

    // cleanup
    close(sub_fd);
    if (unlink(argv[2]) != 0) {
        fprintf(stderr, "ERR failed deleting FIFO %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
