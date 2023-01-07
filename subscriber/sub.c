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
        fprintf(stderr, "usage: sub <register_pipe_name> <pipe_name> \
                <box_name>\n");
        exit(EXIT_FAILURE);
    }

    // request to be sent to mbroker
    registration_request_t req;

    if (registration_request_init(&req, SUB_REGISTER_OP, argv[2], argv[3]) !=
        0) {
        fprintf(stderr, "Error initializing subscriber request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "subscriber: Error creating FIFO\n");
        exit(EXIT_FAILURE);
    }

    // open writting pipe end
    int mbroker_fd = open(argv[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "Error connecting to mbroker\n");
         if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed to delete FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    // make request to mbroker
    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "Error registering subscriber\n");
        close(mbroker_fd);
         if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed to delete FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    // open subscriber assigned FIFO
    int sub_fd = open(req.pipe_name, O_WRONLY);
    if (sub_fd == -1) {
        fprintf(stderr, "subscriber: Error opening pub FIFO\n");
         if (unlink(argv[2]) != 0) {
            fprintf(stderr, "ERR failed to delete FIFO %s", argv[2]);
        }
        exit(EXIT_FAILURE);
    }

    return 0;

}
