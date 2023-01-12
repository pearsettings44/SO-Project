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

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: pub <register_pipe_name> <pipe_name> \
                <box_name>\n");
        exit(EXIT_FAILURE);
    }

    // registration request to be sent to mbroker
    registration_request_t req;

    // initialize registration request
    if (registration_request_init(&req, PUB_REGISTER_OP, argv[2], argv[3]) !=
        0) {
        fprintf(stderr, "Error initializing publisher request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    // create FIFO used for communication with  mbroker
    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "publisher: Error creating FIFO\n");
        exit(EXIT_FAILURE);
    }

    // open mbroker registration pipe
    int mbroker_fd = open(argv[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "ERR opening FIFO %s\n", argv[1]);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    // make request to mbroker
    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "ERR failed registering publisher\n");
        close(mbroker_fd);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    // open communication FIFO
    int pub_fd = open(req.pipe_name, O_WRONLY);
    if (pub_fd == -1) {
        fprintf(stderr, "ERR failed opening FIFO %s\n", req.pipe_name);
        if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed to delete FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

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
            fprintf(stderr, "ERR initializing publisher request\n");
            close(pub_fd);
            if (unlink(argv[2]) != 0) {
                fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
            }
            exit(EXIT_FAILURE);
        }

        // send request to mbroker
        if (publisher_request_send(pub_fd, &p_req) != 0) {
            fprintf(stderr, "ERR couldn't write or partial write to FIFO %s\n", req.pipe_name);
            close(pub_fd);
            if (unlink(argv[2]) != 0) {
                fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
            }
            exit(EXIT_FAILURE);
        }
    }

    close(pub_fd);
    if (unlink(argv[2]) != 0) {
        fprintf(stderr, "ERR failed deleting FIFO %s", argv[2]);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
