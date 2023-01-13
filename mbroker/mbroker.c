#include "mbroker.h"
#include "betterassert.h"
#include "box.h"
#include "logging.h"
#include "operations.h"
#include "wrapper.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

box_t *mbroker_boxes;
allocation_state_t *boxes_bitmap;
static int box_count;

void sigpipe_handler(int sig) {
    (void)sig;
    // closed pipes are handled by EPIPE return value
    // this is used simply to overwrite default SIGPIPE behaviour
}

void *pub_thread(void *args) {
    handle_publisher((registration_request_t *)args);

    return NULL;
}

void *sub_thread(void *args) {
    handle_subscriber((registration_request_t *)args);

    return NULL;
}

int init_mbroker() {
    tfs_params params = tfs_default_params();
    params.max_inode_count = MAX_BOX_COUNT;

    if (tfs_init(&params) != 0) {
        fprintf(stderr, "ERR: Failed initializing TFS instance\n");
        return -1;
    }

    if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        fprintf(stderr, "ERR couldn't overwritte SIGPIPE default handler\n");
        return -1;
    }

    mbroker_boxes = malloc(sizeof(box_t) * MAX_BOX_COUNT);
    boxes_bitmap = malloc(sizeof(allocation_state_t) * MAX_BOX_COUNT);

    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        pthread_cond_init(&mbroker_boxes[i].condition, NULL);
        pthread_mutex_init(&mbroker_boxes[i].mutex, NULL);
    }

    return 0;
}

int mbroker_destroy() {
    if (tfs_destroy() != 0) {
        fprintf(stderr, "ERR Failed destroying TFS instance\n");
        return -1;
    }

    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        // TODO destroy blocks mutexes and cond variables
    }

    free(mbroker_boxes);
    free(boxes_bitmap);

    return 0;
}

/**
 * This is the main thread that will handle registration requests by the
 * various clients.
 */
int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: mbroker <pipename> <max-sessions>\n");
        exit(EXIT_FAILURE);
    }

    if (init_mbroker() != 0) {
        fprintf(stderr, "ERR: Failed initializing mbroker\n");
        exit(EXIT_FAILURE);
    }

    // create known registration pipe for clients
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        fprintf(stderr, "mbroker: couldn't create FIFO %s\n", argv[1]);
        tfs_destroy();
        exit(EXIT_FAILURE);
    }

    // open pipe
    int mbroker_fd = open(argv[1], O_RDONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "Error opening pipe %s\n", argv[1]);
        tfs_destroy();
        exit(EXIT_FAILURE);
    }

    // workaround SIGPIPE
    int dummy_fd = open(argv[1], O_WRONLY);
    if (dummy_fd == -1) {
        fprintf(stderr, "ERR failed to open pipe %s\n", argv[1]);
        tfs_destroy();
        exit(EXIT_FAILURE);
    }

    /**
     * Registration requests expected from the clients
     *
     * Request format:
     * [ op_code (uint8_t) | pipe_name (char[256]) | box_name (char[32]) ]
     *
     * Accepted op_codes are handled by the switch case, if invalid op_codes are
     * provided nothing will be done and an error will be printed to stderr
     */
    registration_request_t req;

    // this is the main loop
    while (1) {
        // read requests on registration pipe
        ssize_t ret = read(mbroker_fd, &req, sizeof(req));
        // pipe closed somewhere, procced
        if (ret == 0) {
            continue;
            // partial read or error reading
        } else if (ret != sizeof(req)) {
            fprintf(stderr, "ERR Failed to read request\n");
            continue;
        }

        // insert_queue(req);

        pthread_t tids[2];

        switch (req.op_code) {
        case 1:
            fprintf(stderr, "OP_CODE 1: received\n");
            // handle_publisher(&req);
            pthread_create(&tids[0], NULL, pub_thread, (void *)&req);
            break;
        case 2:
            fprintf(stderr, "OP_CODE 2: received\n");
            // handle_subscriber(&req);
            pthread_create(&tids[1], NULL, sub_thread, (void *)&req);
            break;
        case 3:
            fprintf(stderr, "OP_CODE 3: received\n");
            handle_manager(&req);
            break;
        case 5:
            fprintf(stderr, "OP_CODE 5: received\n");
            handle_manager(&req);
            break;
        case 7:
            fprintf(stderr, "OP_CODE 7: received\n");
            handle_list(&req);
            break;
        default:
            fprintf(stderr, "Ignoring unknown OP_CODE %d\n", req.op_code);
            break;
        }
    }

    if (mbroker_destroy() != 0) {
        fprintf(stderr, "ERR Failed terminating mbroker\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

/**
 * Ran by thread that handles a publisher connection.
 * If there is an issue with the specified box the pipe will be closed prompting
 * a SIGPIPE on the client side.
 *
 * Possible issues:
 * - Box doesn't exist
 * - Box already as a publisher writting to it
 */
int handle_publisher(registration_request_t *req) {
    // unlock publisher process, even if request is invalid
    int publisher_fd = open(req->pipe_name, O_RDONLY);
    if (publisher_fd == -1) {
        fprintf(stderr, "ERROR Failed opening publisher pipe\n");
        return -1;
    }

    // get box, if it doesn't exist close the pipe
    box_t *box = get_box(req->box_name);
    if (box == NULL) {
        fprintf(stderr, "ERR Box doesn't exist\n");
        close(publisher_fd);
        return -1;
    }

    // lock box mutex
    if (mutex_lock(&box->mutex) != 0) {
        close(publisher_fd);
        return -1;
    }

    // check if there is a publisher writting to box
    if (box->n_publishers == 1) {
        fprintf(stderr, "ERR publisher already writting to box\n");
        close(publisher_fd);
        return -1;
    }

    // open box file in TFS
    int box_fd = tfs_open(box->name, TFS_O_APPEND);
    if (box_fd == -1) {
        fprintf(stderr, "ERR Couldn't access box (TFS)\n");
        close(publisher_fd);
        return -1;
    }

    /**
     * Request to be received from client
     * Request format:
     *  [ op_code = 9 (uint8_t) | message (char[1024]) )
     */
    publisher_request_t pub_r;

    // mark new publisher in box
    box->n_publishers += 1;

    // unlock box mutex
    if (mutex_unlock(&box->mutex) != 0) {
        close(publisher_fd);
        return -1;
    }

    while (1) {
        // read client request
        ssize_t ret = read(publisher_fd, &pub_r, sizeof(pub_r));
        // if EOF received, end session
        if (ret == 0) {
            break;
            // couldn't read request (partial read or error)
        } else if (ret != sizeof(pub_r)) {
            break;
        }

        // lock box before writting operation
        if (mutex_lock(&box->mutex) != 0) {
            tfs_close(box_fd);
            close(publisher_fd);
            return -1;
        }

        // write message contents into box
        ret = write_message(box_fd, pub_r.message);
        // if unsucessful write
        if (ret == -1) {
            mutex_unlock(&box->mutex);
            break;
        }
        // update box info
        box->size += (size_t)ret + 1;
        if (mutex_unlock(&box->mutex) != 0) {
            tfs_close(box_fd);
            close(publisher_fd);
            return -1;
        }
        // signal threads waiting on this box's conditional variable
        if (cond_broadcast(&box->condition) != 0) {
            tfs_close(box_fd);
            close(publisher_fd);
            return -1;
        }
    }

    // lock mutex for update on publishers count
    if (mutex_lock(&box->mutex) != 0) {
        tfs_close(box_fd);
        close(publisher_fd);
        return -1;
    }
    box->n_publishers--;
    // unlock mutex
    if (mutex_unlock(&box->mutex) != 0) {
        tfs_close(box_fd);
        close(publisher_fd);
        return -1;
    }

    tfs_close(box_fd);
    // close pipe
    close(publisher_fd);

    return 0;
}

/**
 * Handle a manager session to create or delete boxes and responds back to the
 * client.
 *
 * Response to the manager client will be sent over the pipe.
 * Possible error messages are:
 * - Box already exists if trying to create a duplicate one
 * - Box doesn't exist if trying to delete a non existing box
 *
 * All other possible errors are not in the scope of the project
 */
int handle_manager(registration_request_t *req) {
    // open pipe
    int manager_fd = open(req->pipe_name, O_WRONLY);
    if (manager_fd == -1) {
        fprintf(stderr, "ERROR Failed opening manager pipe\n");
        return -1;
    }

    /**
     * Response to be sent to the client.
     *
     * Response format:
     * [ op_code (uint8_t) | return_code (uint32_t) | error_message(char[1024])
     * ]
     *
     * op_code = 4 if box creation || op_code = 6 if box deletion
     */
    manager_response_t resp;

    // initialize response struct
    if (manager_response_init(&resp, req->op_code + 1, 0, NULL) != 0) {
        fprintf(stderr, "ERROR Failed creating response\n");
        close(manager_fd);
        return -1;
    }

    switch (req->op_code) {
    case CREATE_BOX_OP:
        resp.ret_code = create_box(&resp, req->box_name);
        if (resp.ret_code == 0) {
            box_count++;
        }
        break;
    case DELETE_BOX_OP:
        resp.ret_code = delete_box(&resp, req->box_name);
        if (resp.ret_code == 0) {
            box_count--;
        }
        break;
    default:
        break;
    }

    // send response to the client
    if (manager_response_send(manager_fd, &resp) != 0) {
        fprintf(stderr, "ERROR Failed sending response\n");
        exit(EXIT_FAILURE);
    }

    close(manager_fd);

    return 0;
}

/**
 * Ran by thread that will handle a subscriber session
 *
 * If specified box doesn't exist, the pipe will be closed
 */
int handle_subscriber(registration_request_t *req) {
    // open pipe to send responses
    int sub_fd = open(req->pipe_name, O_WRONLY);
    if (sub_fd == -1) {
        fprintf(stderr, "ERR couldn't open subscriber pipe %s\n",
                req->pipe_name);
        return -1;
    }

    // get box
    box_t *box = get_box(req->box_name);
    if (box == NULL) {
        fprintf(stderr, "ERR box doesn't exist\n");
        close(sub_fd);
        return -1;
    }

    // open file in TFS
    int box_fd = tfs_open(box->name, 0);
    if (box_fd == -1) {
        fprintf(stderr, "ERR Couldn't open box (TFS)\n");
        close(sub_fd);
        return -1;
    }

    // lock box mutex
    if (mutex_lock(&box->mutex) != 0) {
        close(sub_fd);
        tfs_close(box_fd);
        return -1;
    }

    // add a subscriber to box
    box->n_subscribers++;

    // unlock box mutex
    if (mutex_unlock(&box->mutex) != 0) {
        close(sub_fd);
        tfs_close(box_fd);
        return -1;
    }

    // response to be sent to client
    subscriber_response_t sub_resp;

    char BUFF[MESSAGE_LENGTH];
    size_t total_read = 0;
    while (1) {
        if (mutex_lock(&box->mutex) != 0) {
            close(sub_fd);
            tfs_close(box_fd);
            return -1;
        }
        // read a message from the box into BUFF
        if (total_read < box->size) {
            ssize_t ret = read_message(box_fd, BUFF);
            if (ret == -1) {
                // TO DO add a conditional variable thingy somewhere in here
                // (inside the loop) continues looping untill SIGPIPE recieved
                break;
            }

            total_read += (size_t)ret;

            // initialize client response
            if (subscriber_response_init(&sub_resp, BUFF) != 0) {
                fprintf(stderr,
                        "ERR Couldn't initialize subscriber response\n");
                break;
            }

            // send response
            int r = subscriber_response_send(sub_fd, &sub_resp);

            if (r != 0) {
                // if EPIPE
                if (r == -1) {
                    fprintf(stderr, "Client ended the session\n");
                } else {
                    fprintf(stderr,
                            "ERR Couldn't write or patial write to FIFO %s\n",
                            req->pipe_name);
                }
                break;
            }

            if (mutex_unlock(&box->mutex) != 0) {
                close(sub_fd);
                tfs_close(box_fd);
                return -1;
            }
        } else {
            if (cond_wait(&box->condition, &box->mutex) != 0) {
                close(sub_fd);
                tfs_close(box_fd);
                return -1;
            }
            if (mutex_unlock(&box->mutex) != 0) {
                close(sub_fd);
                tfs_close(box_fd);
                return -1;
            }
        }
    }

    box->n_subscribers--;
    close(sub_fd);
    tfs_close(box_fd);

    if (mutex_unlock(&box->mutex) != 0) {
        return -1;
    }

    return 0;
}

int handle_list(registration_request_t *req) {
    int manager_fd = open(req->pipe_name, O_WRONLY);
    if (manager_fd == -1) {
        fprintf(stderr, "ERR Failed opening pipe %s\n", req->pipe_name);
        exit(EXIT_FAILURE);
    }

    box_t *boxes = get_boxes_list();
    allocation_state_t *bitmap = get_bitmap();

    // iterate through boxes and send them to the manager
    int j = 0;
    for (int i = 0; i < MAX_BOX_COUNT; i++) {
        if (j == box_count) {
            break;
        }
        if (bitmap[i] == TAKEN) {
            j++;
            list_manager_response_t resp;
            box_t box = boxes[i];
            uint8_t last = (j == box_count) ? 1 : 0;
            if (list_manager_response_init(&resp, last, box.name,
                                           box.size, box.n_publishers,
                                           box.n_subscribers) != 0) {
                fprintf(stderr, "ERROR Failed initializing response\n");
                exit(EXIT_FAILURE);
            }

            if (list_manager_response_send(manager_fd, &resp) != 0) {
                fprintf(stderr, "ERROR Failed sending response\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    close(manager_fd);

    return 0;
}

/**
 * Return an mbroker's box with given string
 *
 */
box_t *get_box(char *name) {
    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        if (boxes_bitmap[i] == TAKEN) {
            if (strcmp(name, mbroker_boxes[i].name + 1) == 0) {
                return &mbroker_boxes[i];
            }
        }
    }

    return NULL;
}

/**
 * Return the array storing all mbroker's boxes
 *
 * Declared so box.c has access to the data
 */
box_t *get_boxes_list() { return mbroker_boxes; }

/**
 * Return the bitmap of the boxes in mbroker
 *
 * Declared so box.c has access to the data
 */
allocation_state_t *get_bitmap() { return boxes_bitmap; }