#include "mbroker.h"
#include "betterassert.h"
#include "box.h"
#include "logging.h"
#include "operations.h"
#include "producer-consumer.h"
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
pthread_mutex_t mbroker_boxes_lock = PTHREAD_MUTEX_INITIALIZER;
static int box_count;

void sigpipe_handler(int sig) {
    (void)sig;
    // closed pipes are handled by EPIPE return value
    // this is used simply to overwrite default SIGPIPE behaviour
}

// worked thread
void *worker_thread_fn(void *queue) {
    while (1) {
        // get request
        registration_request_t *req = pcq_dequeue((pc_queue_t *)queue);
        // handle request
        int ret = requests_handler(req);
        if (ret == -1) {
            WARN("Request handler returned an error");
        }
        // free memory associated
        free(req);
    }
    return NULL;
}

int init_mbroker() {
    tfs_params params = tfs_default_params();
    params.max_inode_count = MAX_BOX_COUNT;

    if (tfs_init(&params) != 0) {
        fprintf(stderr, "ERROR: Failed initializing TFS instance\n");
        return -1;
    }

    if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        fprintf(stderr, "ERROR: Couldn't overwritte SIGPIPE default handler\n");
        return -1;
    }

    mbroker_boxes = malloc(sizeof(box_t) * MAX_BOX_COUNT);

    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        mbroker_boxes[i].alloc_state = NOT_USED;
        pthread_cond_init(&mbroker_boxes[i].condition, NULL);
        pthread_mutex_init(&mbroker_boxes[i].mutex, NULL);
    }

    return 0;
}

int mbroker_destroy() {
    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        pthread_cond_destroy(&mbroker_boxes[i].condition);
        pthread_mutex_destroy(&mbroker_boxes[i].mutex);
    }

    if (tfs_destroy() != 0) {
        fprintf(stderr, "ERROR: Failed destroying TFS instance\n");
        return -1;
    }

    free(mbroker_boxes);
    pthread_mutex_destroy(&mbroker_boxes_lock);

    return 0;
}

/**
 * This is the main thread that will handle registration requests by the
 * various clients.
 */
int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0) {
        fprintf(stdout, "usage: mbroker <pipename> <max-sessions>\n");
        exit(EXIT_FAILURE);
    }

    if (init_mbroker() != 0) {
        fprintf(stderr, "FATAL: Failed initializing mbroker\n");
        exit(EXIT_FAILURE);
    }

    // create known registration pipe for clients
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        fprintf(stderr, "FATAL: Couldn't create FIFO %s\n", argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // open pipe
    int mbroker_fd = open(argv[1], O_RDONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, "FATAL: Couldn't open pipe %s\n", argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // workaround SIGPIPE
    int dummy_fd = open(argv[1], O_WRONLY);
    if (dummy_fd == -1) {
        fprintf(stderr, "FATAL: Couldn't open pipe %s\n", argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    int sessions = atoi(argv[2]);

    pc_queue_t requests_queue;

    if (pcq_create(&requests_queue, (size_t)sessions) != 0) {
        fprintf(stderr, "FATAL: Failed creating pcq\n");
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    /**
     * Unbound buffer where registration requests are stored for all worker
     * threads.
     * 
    */
    pthread_t tid[sessions];

    for (unsigned int i = 0; i < sessions; ++i) {
        if (pthread_create(&tid[i], NULL, worker_thread_fn,
                           (void *)&requests_queue) != 0) {
            fprintf(stderr, "FATAL: Failed launching thread pool\n");
            mbroker_destroy();
            exit(EXIT_FAILURE);
        }
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
    registration_request_t *req;

    // main loop of the program
    while (1) {
        req = malloc(sizeof(*req));
        // read requests from registration pipe
        ssize_t ret = read(mbroker_fd, req, sizeof(*req));
        // pipe closed somewhere, continue accepting requests
        if (ret == 0) {
            continue;
        // partial read or error reading
        } else if (ret != sizeof(*req)) {
            fprintf(stderr, "ERROR: Failed to read request\n");
            continue;
        }
        LOG("Request received");
        // add request to queue
        pcq_enqueue(&requests_queue, req);
    }

    /**
     * Wait on all threads before exiting. No ending method was
     * specified in the project paper, so we would simply wait
    */
    for (int i = 0; i < sessions; ++i) {
        if (pthread_join(tid[i], NULL) != 0) {
            fprintf(stderr, "FATAL: Coudln't join thread %ld\n", tid[i]);
        };
    }

    if (mbroker_destroy() != 0) {
        fprintf(stderr, "FATAL: Failed terminating mbroker\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

/**
 * Handle a registration request from various clients
*/
int requests_handler(registration_request_t *req) {
    // LOG(req->op_code);
    // fprintf(stdout, "OP_CODE %d: received\n", req->op_code);
    switch (req->op_code) {
    case 1:
        handle_publisher(req);
        break;
    case 2:
        handle_subscriber(req);
        break;
    case 3:
        handle_manager(req);
        break;
    case 5:
        handle_manager(req);
        break;
    case 7:
        handle_list(req);
        break;
    default:
        // fprintf(stderr, "Ignoring unknown OP_CODE %d\n", req->op_code);
        break;
    }

    return 0;
}

/**
 * Ran by thread that handles a publisher connection.
 * If there is an issue with the specified box the pipe will be closed prompting
 * a SIGPIPE on the client side.
 *
 * Possible issues:
 * - Box doesn't exist
 * - Box already has a publisher writting to it
 */
int handle_publisher(registration_request_t *req) {
    INFO("Handling publisher session")
    // unlock publisher process, even if request is invalid
    int publisher_fd = open(req->pipe_name, O_RDONLY);
    if (publisher_fd == -1) {
        fprintf(stderr, "ERROR: Failed opening publisher pipe %s\n",
                req->pipe_name);
        return -1;
    }

    // get box, if it doesn't exist close the pipe
    box_t *box = get_box(req->box_name);
    if (box == NULL) {
        fprintf(stderr, "ERROR: Box %s doesn't exist\n", req->box_name);
        close(publisher_fd);
        return -1;
    }

    // lock box mutex
    if (pthread_mutex_lock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL: Couldn't lock box %s mutex\n", box->name);
        exit(EXIT_FAILURE);
    }

    // check if there is a publisher writting to box
    if (box->n_publishers == 1) {
        fprintf(stderr, "ERROR: Another publisher already writting to box %s\n",
                box->name);
        close(publisher_fd);
        return -1;
    }

    // open box file in TFS
    int box_fd = tfs_open(box->name, TFS_O_APPEND);
    if (box_fd == -1) {
        fprintf(stderr, "ERROR: Couldn't open box %s data\n", box->name);
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
    if (pthread_mutex_unlock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL: Couldn't unlock box %s mutex\n", box->name);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // read publisher request
        ssize_t ret = read(publisher_fd, &pub_r, sizeof(pub_r));
        // if EOF received, end session
        if (ret == 0) {
            break;
            // couldn't read request (partial read or error)
        } else if (ret != sizeof(pub_r)) {
            fprintf(stderr, "ERROR: Couldn't read publisher request\n");
            break;
        }

        // always check if box is not deleted while awaiting for a request
        if (box->alloc_state == NOT_USED) {
            break;
        }

        // lock box before writting operation
        if (pthread_mutex_lock(&box->mutex) != 0) {
            fprintf(stderr, "FATAL: Couldn't lock box %s mutex\n", box->name);
            exit(EXIT_FAILURE);
        }

        // write message contents into box
        ret = write_message(box_fd, pub_r.message);
        // if unsucessful write
        if (ret == -1) {
            if (pthread_mutex_unlock(&box->mutex) != 0) {
                fprintf(stderr, "FATAL: Couldn't unlock box %s mutex\n",
                        box->name);
                exit(EXIT_FAILURE);
            }
            break;
        }
        // update box info
        box->size += (size_t)ret + 1;
        if (pthread_mutex_unlock(&box->mutex) != 0) {
            fprintf(stderr, "FATAL: Couldn't unlock box %s mutex\n", box->name);
            exit(EXIT_FAILURE);
        }
        // signal threads waiting on this box's conditional variable
        if (pthread_cond_broadcast(&box->condition) != 0) {
            fprintf(
                stderr,
                "FATAL: Couldn't broadcast on box %s conditional variable\n",
                box->name);
            exit(EXIT_FAILURE);
        }
    }

    // lock mutex for update on publishers count
    if (pthread_mutex_lock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL Couldn't lock box %s mutex\n", box->name);
        exit(EXIT_FAILURE);
    }
    box->n_publishers--;
    // unlock mutex
    if (pthread_mutex_unlock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL: Couldn't unlock box %s mutex\n", box->name);
        exit(EXIT_FAILURE);
    }

    tfs_close(box_fd);
    // close pipe
    close(publisher_fd);

    INFO("Successfully finished handling publisher session");
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
    INFO("Handling manager session")
    // open pipe
    int manager_fd = open(req->pipe_name, O_WRONLY);
    if (manager_fd == -1) {
        fprintf(stderr, "ERR Failed opening manager pipe\n");
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
        fprintf(stderr, "ERR Failed creating response\n");
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
        fprintf(stderr, "ERR Failed sending response\n");
        return -1;
    }

    close(manager_fd);

    INFO("Sucessfully finished handling manager session");
    return 0;
}

/**
 * Ran by thread that will handle a subscriber session
 *
 * If specified box doesn't exist, the pipe will be closed
 */
int handle_subscriber(registration_request_t *req) {
    INFO("Handling subscriber session")
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
    if (pthread_mutex_lock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL Couldn't lock box's mutex\n");
        exit(EXIT_FAILURE);
    }

    // add a subscriber to box
    box->n_subscribers++;

    // unlock box mutex
    if (pthread_mutex_unlock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL Couldn't unlock box's mutex\n");
        exit(EXIT_FAILURE);
    }

    // response to be sent to client
    subscriber_response_t sub_resp;

    char BUFF[MESSAGE_LENGTH];
    size_t total_read = 0;
    while (1) {
        if (pthread_mutex_lock(&box->mutex) != 0) {
            fprintf(stderr, "FATAL Couldn't lock box's mutex \n");
            exit(EXIT_FAILURE);
        }
        // read a message from the box into BUFF
        if (total_read >= box->size) {
            // wait untill there are messages to be read
            if (pthread_cond_wait(&box->condition, &box->mutex) != 0) {
                fprintf(stderr,
                        "FATAL Couldn't wait on box's conditional variable\n");
                exit(EXIT_FAILURE);
            }
        }
        ssize_t ret = read_message(box_fd, BUFF);
        // session closed (EPIPE read)
        if (ret == -1) {
            break;
        }
        // box deleted mid operation
        if (box->alloc_state == NOT_USED) {
            break;
        }

        total_read += (size_t)ret;

        // initialize client response
        if (subscriber_response_init(&sub_resp, BUFF) != 0) {
            fprintf(stderr, "ERR Couldn't initialize subscriber response\n");
            break;
        }

        // send response
        int r = subscriber_response_send(sub_fd, &sub_resp);
        if (r != 0) {
            // if EPIPE
            if (r == -1) {
                if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
                    fprintf(stderr, "FATAL Couldn't assign SIGPIPE handler\n");
                    exit(EXIT_FAILURE);
                };
            } else {
                fprintf(stderr,
                        "ERR Couldn't write or patial write to FIFO %s\n",
                        req->pipe_name);
            }
            break;
        }
        if (pthread_mutex_unlock(&box->mutex) != 0) {
            fprintf(stderr, "FATAL Couldn't unlock box's mutex\n");
            exit(EXIT_FAILURE);
        }
    }

    box->n_subscribers--;
    close(sub_fd);
    tfs_close(box_fd);

    if (pthread_mutex_unlock(&box->mutex) != 0) {
        fprintf(stderr, "FATAL Couldn't unlock box's mutex\n");
        exit(EXIT_FAILURE);
    }

    INFO("Successfully finished handling subscriber session");
    return 0;
}

int handle_list(registration_request_t *req) {
    INFO("Handling manager listing session");
    int manager_fd = open(req->pipe_name, O_WRONLY);
    if (manager_fd == -1) {
        fprintf(stderr, "ERR Failed opening pipe %s\n", req->pipe_name);
        return -1;
    }

    // iterate through boxes and send them to the manager
    int j = 0;
    for (int i = 0; i < MAX_BOX_COUNT; i++) {
        if (j == box_count) {
            break;
        }
        if (mbroker_boxes[i].alloc_state == USED) {
            j++;
            list_manager_response_t resp;
            box_t box = mbroker_boxes[i];
            uint8_t last = (j == box_count) ? 1 : 0;
            if (list_manager_response_init(&resp, last, box.name, box.size,
                                           box.n_publishers,
                                           box.n_subscribers) != 0) {
                fprintf(stderr, "ERROR Failed initializing response\n");
                break;
            }

            if (list_manager_response_send(manager_fd, &resp) != 0) {
                fprintf(stderr, "ERROR Failed sending response\n");
                break;
            }
        }
    }

    close(manager_fd);

    INFO("Successfully finished handling manager listing session");
    return 0;
}

/**
 * Return an mbroker's box with given string
 *
 */
box_t *get_box(char *name) {
    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        if (mbroker_boxes[i].alloc_state == USED) {
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
box_t *get_mbroker_boxes_ref() { return mbroker_boxes; }

/**
 * Returns mbroker_boxes associated mutex
 */
pthread_mutex_t *get_mbroker_boxes_lock() { return &mbroker_boxes_lock; }
