#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "requests.h"
#include "logging.h"

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: sub <register_pipe_name> <pipe_name> \
                <box_name>\n");
        exit(EXIT_FAILURE);
    }

    return -1;
}
