#include "logging.h"
#include "requests.h"
#include "response.h"
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static void print_usage() {
    fprintf(stderr,
            "usage: \n"
            "manager <register_pipe_name> <pipe_name> create <box_name>\n"
            "manager <register_pipe_name> <pipe_name> remove <box_name>\n"
            "manager <register_pipe_name> <pipe_name> list\n");
}

int create_delete_box(uint8_t, char **);
int list_boxes(char **);

int main(int argc, char **argv) {
    if ((argc < 4 && (strcmp(argv[3], "list") != 0)) ||
        strcmp(argv[1], "--help") == 0) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[3], "create") == 0) {
        create_delete_box(CREATE_BOX_OP, argv);
    } else if (strcmp(argv[3], "delete") == 0) {
        create_delete_box(DELETE_BOX_OP, argv);
    } else if (strcmp(argv[3], "list") == 0) {
        list_boxes(argv);
    } else {
        print_usage();
        exit(EXIT_FAILURE);
    }

    return 0;
}

int create_delete_box(uint8_t op_code, char **args) {
    registration_request_t req;

    if (registration_request_init(&req, op_code, args[2], args[3]) != 0) {
        fprintf(stderr, "manager: Failed initializing request\n");
        exit(EXIT_FAILURE);
    }

    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "manager: Failed creating FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    int mbroker_fd = open(args[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "manager: Failed opening pipe %s\n", args[1]);
        exit(EXIT_FAILURE);
    }

    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "manager: Failed making request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    int manager_fd = open(req.pipe_name, O_RDONLY);
    if (manager_fd == -1) {
        fprintf(stderr, "manager: Failed opening pipe %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    manager_response_t resp;

    ssize_t ret = read(manager_fd, &resp, sizeof(resp));
    if (ret != sizeof(resp)) {
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Response from server %d, %d", resp.ret_code, resp.op_code);

    close(manager_fd);

    return 0;
}

int list_boxes(char **args) {
    registration_request_t req;

    if (registration_request_init(&req, LIST_BOX_OP, args[2], NULL)) {
        fprintf(stderr, "manager: Failed initializing request");
        exit(EXIT_FAILURE);
    }

    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "manager: Failed creating FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    int mbroker_fd = open(args[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "manager: Failed opening pipe %s\n", args[1]);
        exit(EXIT_FAILURE);
    }

    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "manager: Failed making request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    close(mbroker_fd);

    return 0;
}