#include "logging.h"
#include "mbroker.h"
#include "requests.h"
#include "response.h"
#include <fcntl.h>
#include <mbroker.h>
#include <stdbool.h>
#include <stdlib.h>
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

int cmpstr(const void *a, const void *b) {
    return strcmp(((list_manager_response_t *)a)->box_name,
                  ((list_manager_response_t *)b)->box_name);
}

int main(int argc, char **argv) {
    if ((argc < 5 && (strcmp(argv[3], "list") != 0)) ||
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

    if (registration_request_init(&req, op_code, args[2], args[4]) != 0) {
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
        fprintf(stderr, "manager, Couldn't read or patial read from pipe %s\n",
                req.pipe_name);
        close(manager_fd);
        exit(EXIT_FAILURE);
    }

    if (resp.ret_code == -1) {
        fprintf(stderr, "ERR %s", resp.error_message);
    } else {
        fprintf(stdout, "OK\n");
    }

    close(manager_fd);
    if (unlink(req.pipe_name) != 0) {
        fprintf(stderr, "ERROR: failed to delete FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

int list_boxes(char **args) {
    registration_request_t req;

    if (registration_request_init(&req, LIST_BOX_OP, args[2], NULL)) {
        fprintf(stderr, "ERROR: Failed initializing request");
        exit(EXIT_FAILURE);
    }

    if (registration_request_mkfifo(&req) != 0) {
        fprintf(stderr, "ERROR: Failed creating FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    int mbroker_fd = open(args[1], O_WRONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "ERROR: Failed opening pipe %s\n", args[1]);
        exit(EXIT_FAILURE);
    }

    if (registration_request_send(mbroker_fd, &req) != 0) {
        fprintf(stderr, "ERROR: Failed making request to mbroker\n");
        exit(EXIT_FAILURE);
    }

    // open communication FIFO
    int list_fd = open(req.pipe_name, O_RDONLY);
    if (list_fd == -1) {
        fprintf(stderr, "ERROR: failed opening FIFO %s\n", args[2]);
        if (unlink(args[2]) != 0) {
            fprintf(stderr, "ERROR: failed deleting FIFO %s\n", args[2]);
        }
        exit(EXIT_FAILURE);
    }

    list_manager_response_t list_resp;
    // array to store the received responses
    list_manager_response_t boxes[MAX_BOX_COUNT];
    int box_count = 0;
    while (1) {
        ssize_t ret = read(list_fd, &list_resp, sizeof(list_resp));
        // pipe closed without any read -> no boxes
        if (ret == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
            break;
        } else if (ret != sizeof(list_resp)) {
            fprintf(stderr, "ERROR: couldn't read response from pipe\n");
            break;
        }

        // add box to boxes array
        boxes[box_count] = list_resp;
        box_count++;

        // if there are no more boxes to read
        if (list_resp.last_flag == 1) {
            break;
        }
    }

    // sort boxes by name
    qsort(boxes, (size_t)box_count, sizeof(list_manager_response_t), cmpstr);

    for (int i = 0; i < box_count; i++) {
        // list_manager_response_t box = boxes[i];
        fprintf(stdout, "%s %zu %zu %zu\n", boxes[i].box_name + 1,
                boxes[i].box_size, boxes[i].n_publishers,
                boxes[i].n_subscribers);
    }

    close(list_fd);
    if (unlink(req.pipe_name) != 0) {
        fprintf(stderr, "ERROR: failed to delete FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}