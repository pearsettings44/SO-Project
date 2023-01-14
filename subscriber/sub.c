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

// flag that will signal main loop of interruption
static volatile sig_atomic_t interrupt_var;

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
