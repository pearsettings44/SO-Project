#include "logging.h"
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
        fprintf(stderr, "ERR failed to delete FIFO %s\n", req.pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
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

    // open communication FIFO
    int man_fd = open(req.pipe_name, O_RDONLY);
    if (man_fd == -1) {
        fprintf(stderr, "ERR failed opening FIFO %s\n", args[2]);
        if (unlink(args[2]) != 0) {
            fprintf(stderr, "ERR failed deleting FIFO %s\n", args[2]);
        }
        exit(EXIT_FAILURE);
    }

    list_manager_response_t man_resp;
    // array to store the names of the boxes
    list_manager_response_t boxes[MAX_BOX_COUNT];
    int box_count = 0;
    while (1) {
        ssize_t ret = read(man_fd, &man_resp, sizeof(man_resp));
        // it failed to read
        if (ret == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
            break;
        } else if (ret != sizeof(man_resp)) {
            fprintf(stderr, "ERR couldn't read response from pipe\n");
            break;
        }

        // add box to boxes array
        boxes[box_count] = man_resp;
        box_count++;

        // if there are no more boxes to read
        if (man_resp.last_flag == 1) {
            break;
        }
    }

    // sort boxes by name
    int compare(const void *a, const void *b) {
        list_manager_response_t *box_a = (list_manager_response_t *)a;
        list_manager_response_t *box_b = (list_manager_response_t *)b;
        return strcmp(box_a->box_name, box_b->box_name);
    }
    qsort(boxes, (size_t)box_count, sizeof(list_manager_response_t), compare);

    // Loop through boxes and print them;
    for (int i = 0; i < box_count; i++) {
        list_manager_response_t box = boxes[i];
        fprintf(stdout, "%s %zu %zu %zu\n", box.box_name + 1, box.box_size,
                box.n_publishers, box.n_subscribers);
    }

    // close(mbroker_fd);
    // if (unlink(req.pipe_name) != 0) {
    //     fprintf(stderr, "ERR failed to delete FIFO %s\n", req.pipe_name);
    //     exit(EXIT_FAILURE);
    // }

    // read = (fd, resp, sizeof(resp);
    // printf(resp.wtv));)

    exit(EXIT_SUCCESS);
}