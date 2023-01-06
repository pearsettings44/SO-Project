#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PUB_PIPE_PATHNAME 256
#define BOX_NAME 32

struct request_t {
    uint8_t op_code;
    char pipe_name[PUB_PIPE_PATHNAME];
    char box_name[BOX_NAME];
};

typedef struct request_t request_t;

int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: mbroker <pipename> <max-sessions>\n");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        fprintf(stderr, "mbroker: couldn't create FIFO \n");
        exit(EXIT_FAILURE);
    }

    int mbroker_fd = open(argv[1], O_RDONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "Error connecting to mbroker\n");
        exit(EXIT_FAILURE);
    }

    request_t req;

    if (read(mbroker_fd, &req, sizeof(req)) != sizeof(req)) {
        fprintf(stderr, "Partial read or no read\n");
        exit(EXIT_FAILURE);
    }

    switch (req.op_code) {
    case 1:
        // register publisher
        while (1)
            ;
        break;
    case 2:
        // register subscriber
    case 3:
        // register manager
    default:
        break;
    }

    // int pub_fd = open(req.pipe_name, O_RDONLY);
    // if (pub_fd == -1) {
    //     fprintf(stderr, "Error connecting to publisher\n");
    //     exit(EXIT_FAILURE);
    // }

    return 0;
}
