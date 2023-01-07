#include <errno.h>
#include <fcntl.h>
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

    // request to be sent to mbroker
    registration_request_t req;

    if (registration_request_init(&req, PUB_REGISTER_OP, argv[2], argv[3]) !=
        0) {
        fprintf(stderr, "Error initializing publisher request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "publisher: Error creating FIFO\n");
        exit(EXIT_FAILURE);
    }

    // open writting pipe end
    int mbroker_fd = open(argv[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "Error connecting to mbroker\n");
        exit(EXIT_FAILURE);
    }

    // make request to mbroker
    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "Error registering publisher\n");
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    // open publisher assigned FIFO
    int pub_fd = open(req.pipe_name, O_WRONLY);
    if (pub_fd == -1) {
        fprintf(stderr, "publisher: Error opening pub FIFO\n");
        exit(EXIT_FAILURE);
    }

    publisher_request_t p_req;
    char BUFFER[MESSAGE_LENGTH];

    while (1) {
        // messages over 1024 chars will be truncated and sent seperately
        char *r = fgets(BUFFER, MESSAGE_LENGTH, stdin);
        // received EOF
        if (r == NULL) {
            close(pub_fd);
            exit(EXIT_FAILURE);
        }

        size_t len = strcspn(BUFFER, "\n");
        // discard \n and set remaining of the string to \0
        memset(BUFFER + len, 0, MESSAGE_LENGTH - len);
        if (publisher_request_init(&p_req, BUFFER) != 0) {
            fprintf(stderr, "ERROR creating request\n");
            exit(EXIT_FAILURE);
        }
        if (publisher_request_send(pub_fd, &p_req) != 0) {
            fprintf(stderr, "ERROR couldn't write or partial write to pipe\n");
            exit(EXIT_FAILURE);
        }
    }

    // TODO handle SIGPIPE signal to unlink files and close file descriptors

    return 0;
}
