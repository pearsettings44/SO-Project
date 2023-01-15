#include "client.h"
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

    int manager_fd =
        connect_to_mbroker(args[1], args[2], args[4], op_code, O_RDONLY);
    char *pipe_name = args[2];

    manager_response_t resp;

    ssize_t ret = read(manager_fd, &resp, sizeof(resp));
    if (ret != sizeof(resp)) {
        fprintf(stderr, PIPE_PARTIAL_RW_ERR_MSG, pipe_name);
        close(manager_fd);
        exit(EXIT_FAILURE);
    }

    if (resp.ret_code == -1) {
        fprintf(stderr, "ERR %s", resp.error_message);
    } else {
        fprintf(stdout, "OK\n");
    }

    close(manager_fd);
    if (unlink(pipe_name) != 0) {
        fprintf(stderr, PIPE_DELETE_ERR_MSG, pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

int list_boxes(char **args) {
    int list_fd =
        connect_to_mbroker(args[1], args[2], NULL, LIST_BOX_OP, O_RDONLY);
    char *pipe_name = args[2];

    // response expected from mbroker
    list_manager_response_t list_resp;
    // array used to store the received responses
    list_manager_response_t boxes[BOX_COUNT_MAX];
    // count of boxes already received
    int box_count = 0;
    while (1) {
        ssize_t ret = read(list_fd, &list_resp, sizeof(list_resp));
        // pipe closed without any read -> no boxes
        if (ret == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
            break;
        } else if (ret != sizeof(list_resp)) {
            fprintf(stderr, PIPE_CREATE_ERR_MSG, pipe_name);
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
        fprintf(stdout, "%s %zu %zu %zu\n", boxes[i].box_name + 1,
                boxes[i].box_size, boxes[i].n_publishers,
                boxes[i].n_subscribers);
    }

    close(list_fd);
    if (unlink(pipe_name) != 0) {
        fprintf(stderr, PIPE_DELETE_ERR_MSG, pipe_name);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}