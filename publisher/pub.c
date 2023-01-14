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

/**
 * SIGPIPE handler that does nothing because broken pipes are handled using
 * EPIPE return value from the read function.
 */
void sigpipe_handler(int sig) { (void)sig; }

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: pub <register_pipe_name> <pipe_name> \
                <box_name>\n");
        exit(EXIT_FAILURE);
    }

    // overwrite default SIGPIPE handler
    if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        fprintf(stderr, SIGNAL_ERR_MSG, "SIGPIPE");
        exit(EXIT_FAILURE);
    }

    // connect client to mbroker (exits if unsucessfull)
    int pub_fd = connect_to_mbroker(argv[1], argv[2], argv[3], PUB_REGISTER_OP,
                                    O_WRONLY);
    // name of FIFO used to communicate with mbroker;
    char *pipe_name = argv[2];

    publisher_request_t p_req;

    char BUFFER[MESSAGE_LENGTH];
    while (1) {
        /* read a message from stdin with maximum 1023 chars
         * or it will be truncated and sent seperately
         */
        char *r = fgets(BUFFER, MESSAGE_LENGTH, stdin);

        // received EOF
        if (r == NULL) {
            break;
        }

        // discard \n and set remainder of the string to \0
        size_t len = strcspn(BUFFER, "\n");
        memset(BUFFER + len, 0, MESSAGE_LENGTH - len);

        // initialize new request to be sent to mbroker
        if (publisher_request_init(&p_req, BUFFER) != 0) {
            fprintf(stderr, REQUEST_INIT_ERR_MSG, PUBLISH_OP_CODE);
            exit_failure(pub_fd, argv[2]);
        }

        // send request to mbroker
        int ret = publisher_request_send(pub_fd, &p_req);
        if (ret != 0) {
            // received EPIPE, meaning mbroker closed pipe on invalid request
            if (ret == -1) {
                fprintf(stderr, INVALID_OP_ERR_MSG);
                // partial write
            } else {
                fprintf(stderr, PIPE_PARTIAL_RW_ERR_MSG, pipe_name);
            }
            exit_failure(pub_fd, pipe_name);
        }
    }

    // finish session
    close(pub_fd);
    if (unlink(pipe_name) != 0) {
        fprintf(stderr, PIPE_DELETE_ERR_MSG, pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}