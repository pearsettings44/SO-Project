#include "logging.h"
#include <string.h>

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        exit(EXIT_FAILURE);
    }
    // print_usage();
    // WARN("unimplemented"); // TODO: implement
    // return -1;
}
