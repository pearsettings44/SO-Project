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
    request_t req;

    if (request_init(&req, argv + 2) != 0) {
        fprintf(stderr, "Error initializing publisher request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    if (request_mkfifo(&req) != 0) {
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
    if (request_register(mbroker_fd, &req, PUB_REGISTER_OP) != 0) {
        fprintf(stderr, "Error registering publisher\n");
        exit(EXIT_FAILURE);
    }

    // open publisher assigned FIFO
    int pub_fd = open(req.pipe_name, O_WRONLY);
    if (pub_fd == -1) {
        fprintf(stderr, "publisher: Error opening pub FIFO\n");
        exit(EXIT_FAILURE);
    }

    // writting loop and singal handling missing

    // char BUFFER[1024];
    // while(fgets(BUFFER, sizeof(BUFFER), stdin) != NULL) {
    //     continue;
    // }

    return 0;
}
