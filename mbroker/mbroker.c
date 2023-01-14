#include "mbroker.h"
#include "betterassert.h"
#include "box.h"
#include "client.h"
#include "logging.h"
#include "operations.h"
#include "producer-consumer.h"
#include "utils.h"
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
static volatile sig_atomic_t interrupt_var;

/**
 * Not specified in statement, but we end mbroker process by marking a flag on
 * receiving a SIGINT
 */
void sigint_handler(int sig) {
    if (sig == SIGINT) {
        interrupt_var = 1;
    }
}

/**
 * Used simply to overwrite default SIGPIPE handler that finishes the process
 */
void sigpipe_handler(int sig) {
    (void)sig;
    // closed pipes are handled by EPIPE return value on read function
}

/**
 * Worker threads main function
 */
void *worker_thread_fn(void *queue) {
    while (1) {
        // get a request from the queue
        registration_request_t *req = pcq_dequeue((pc_queue_t *)queue);
        // handle request
        int ret = requests_handler(req);
        if (ret != 0) {
            WARN(LOG_FAIL_HANDLER);
        }
        // free memory associated with request
        free(req);
    }
    return NULL;
}

/**
 * Initialize mbroker
 */
int init_mbroker() {
    // initialize tfs instance
    tfs_params params = tfs_default_params();
    /**
     * Ideally max open file entries should be a number close to max sessions
     */
    params.max_inode_count = BOX_COUNT_MAX;

    if (tfs_init(&params) != 0) {
        PANIC(FATAL_TFS_INIT);
        return -1;
    }

    // assign SIGINT handler
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        PANIC(FATAL_SIGNAL_MSG);
        return -1;
    }

    // assign SIGPIPE handler (just to overwrite default one)
    if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        PANIC(FATAL_SIGNAL_MSG);
        return -1;
    }

    mbroker_boxes = malloc(sizeof(box_t) * BOX_COUNT_MAX);

    // initialize mutexes and conditional variables
    for (int i = 0; i < BOX_COUNT_MAX; ++i) {
        mbroker_boxes[i].alloc_state = NOT_USED;
        cond_init(&mbroker_boxes[i].condition);
        mutex_init(&mbroker_boxes[i].mutex);
    }

    return 0;
}

/**
 * Safely terminates mbroker's process
 */
int mbroker_destroy() {
    for (int i = 0; i < BOX_COUNT_MAX; ++i) {
        cond_destroy(&mbroker_boxes[i].condition);
        mutex_destroy(&mbroker_boxes[i].mutex);
    }

    if (tfs_destroy() != 0) {
        PANIC(FATAL_TFS_DESTROY);
        return -1;
    }

    free(mbroker_boxes);
    mutex_destroy(&mbroker_boxes_lock);

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

    // initialize mbroker
    if (init_mbroker() != 0) {
        exit(EXIT_FAILURE);
    }

    // create known registration pipe for clients
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        PANIC(FATAL_FIFO_CREATE, argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // open registration pipe
    int mbroker_fd = open(argv[1], O_RDONLY);
    if (mbroker_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // prevent SIGPIPE on client's pipe close
    int dummy_fd = open(argv[1], O_WRONLY);
    if (dummy_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, argv[1]);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // max number of sessions capable of being handled concurrently
    int max_sessions = atoi(argv[2]);

    /**
     * Unbound buffer where registration requests are stored for all worker
     * threads.
     */
    pc_queue_t requests_queue;

    // initialize queue
    if (pcq_create(&requests_queue, (size_t)max_sessions) != 0) {
        PANIC(FATAL_PCQ_INIT);
        mbroker_destroy();
        exit(EXIT_FAILURE);
    }

    // thread pool
    pthread_t tid[max_sessions];

    // launch thread pool
    for (unsigned int i = 0; i < max_sessions; ++i) {
        if (pthread_create(&tid[i], NULL, worker_thread_fn,
                           (void *)&requests_queue) != 0) {
            PANIC(FATAL_THREAD_LAUNCH, i);
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

    // program main loop
    while (1) {
        req = malloc(sizeof(*req));
        // read requests from registration pipe
        ssize_t ret = read(mbroker_fd, req, sizeof(*req));
        // pipe closed somewhere, continue accepting requests
        if (ret == 0) {
            continue;
            // partial read or error reading
        } else if (ret != sizeof(*req) && interrupt_var != 1) {
            fprintf(stderr, PIPE_PARTIAL_RW_ERR_MSG, argv[1]);
            continue;
        }

        /**
         * If proccess received SIGINT terminated. This is a simplification
         * because proccess could get locked if we joined threads
         */
        if (interrupt_var == 1) {
            if (unlink(argv[1]) != 0) {
                fprintf(stderr, PIPE_DELETE_ERR_MSG, argv[1]);
            };
            exit(EXIT_SUCCESS);
        }

        LOG(LOG_REQUEST);
        // add request to queue
        pcq_enqueue(&requests_queue, req);
    }

    /**
     * Wait on all threads before exiting. No ending method was
     * specified in the project paper so this is never reached (althought this
     * could possibly stay locked forever with current specification i.e
     * a subscriber handler thread being stuck on a box conditional variable,
     * thats why we exit on interrupt)
     */
    for (int i = 0; i < max_sessions; ++i) {
        if (pthread_join(tid[i], NULL) != 0) {
            PANIC(FATAL_THREAD_JOIN, tid[i]);
            exit(EXIT_FAILURE);
        };
    }

    if (mbroker_destroy() != 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

/**
 * Handle a registration request from various clients
 */
int requests_handler(registration_request_t *req) {
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
        LOG(LOG_UNKNOWN_REQUEST, req->op_code);
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
    INFO(LOG_PUB_HANDLER)
    // unlock publisher process, even if request is invalid
    int publisher_fd = open(req->pipe_name, O_RDONLY);
    if (publisher_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, req->pipe_name);
        return -1;
    }

    // get box, if it doesn't exist close the pipe
    box_t *box = get_box(req->box_name);
    if (box == NULL) {
        fprintf(stderr, BOX_INVALID_ERR_MSG, req->box_name);
        close(publisher_fd);
        return -1;
    }

    // lock box mutex
    mutex_lock(&box->mutex);
    // check if there is a publisher writting to box
    if (box->n_publishers == 1) {
        fprintf(stderr, BOX_HAS_PUBLISHER_ERR_MSG, box->name);
        close(publisher_fd);
        mutex_unlock(&box->mutex);
        return -1;
    }

    // open box file in TFS
    int box_fd = tfs_open(box->name, TFS_O_APPEND);
    if (box_fd == -1) {
        fprintf(stderr, BOX_TFS_ERR_MSG, box->name);
        close(publisher_fd);
        mutex_unlock(&box->mutex);
        return -1;
    }

    // mark new publisher in box
    box->n_publishers += 1;
    // unlock box mutex
    mutex_unlock(&box->mutex);

    /**
     * Request to be received from client
     * Request format:
     *  [ op_code = 9 (uint8_t) | message (char[1024]) )
     */
    publisher_request_t pub_r;

    while (1) {
        // read publisher request
        ssize_t ret = read(publisher_fd, &pub_r, sizeof(pub_r));
        // if EOF received, end session
        if (ret == 0) {
            break;
            // couldn't read request (partial read or error)
        } else if (ret != sizeof(pub_r)) {
            fprintf(stderr, PIPE_PARTIAL_RW_ERR_MSG, req->pipe_name);
            break;
        }

        // always check if box is not deleted while awaiting for a request
        if (box->alloc_state == NOT_USED) {
            break;
        }

        // lock box before writting operation
        mutex_lock(&box->mutex);
        // write message contents into box
        ret = write_message(box_fd, pub_r.message);
        // if unsucessful write
        if (ret == -1) {
            mutex_unlock(&box->mutex);
            break;
        }
        // update box info
        box->size += (size_t)ret + 1;
        mutex_unlock(&box->mutex);
        // signal threads waiting on this box's conditional variable
        cond_broadcast(&box->condition);
    }

    // lock mutex for update on publishers count
    mutex_lock(&box->mutex);
    box->n_publishers--;
    mutex_unlock(&box->mutex);

    tfs_close(box_fd);
    // close pipe
    close(publisher_fd);

    INFO(LOG_SUCCESS_PUB);
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
    INFO(LOG_MAN_HANDLER)
    // open pipe
    int manager_fd = open(req->pipe_name, O_WRONLY);
    // if pipe opening failed
    if (manager_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, req->pipe_name);
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
        fprintf(stderr, RESPONSE_INIT_ERR_MSG, req->op_code + 1);
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
        fprintf(stderr, RESPONSE_SEND_ERR_MSG, req->op_code + 1);
        return -1;
    }

    close(manager_fd);

    INFO(LOG_SUCCESS_MANAGER);
    return 0;
}

/**
 * Ran by thread that will handle a subscriber session
 *
 * If specified box doesn't exist, the pipe will be closed
 */
int handle_subscriber(registration_request_t *req) {
    INFO(LOG_SUB_HANDLER)
    // open pipe to send responses
    int sub_fd = open(req->pipe_name, O_WRONLY);
    if (sub_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, req->pipe_name);
        return -1;
    }

    // get box
    box_t *box = get_box(req->box_name);
    if (box == NULL) {
        fprintf(stderr, BOX_INVALID_ERR_MSG, req->box_name);
        close(sub_fd);
        return -1;
    }

    mutex_lock(&box->mutex);
    // open file in TFS
    int box_fd = tfs_open(box->name, 0);
    if (box_fd == -1) {
        fprintf(stderr, BOX_TFS_ERR_MSG, box->name);
        close(sub_fd);
        mutex_unlock(&box->mutex);
        return -1;
    }

    // lock box mutex
    box->n_subscribers++;
    mutex_unlock(&box->mutex);

    // response to be sent to client
    subscriber_response_t sub_resp;

    char BUFF[MESSAGE_LENGTH];
    size_t total_read = 0;
    while (1) {
        mutex_lock(&box->mutex);
        // read a message from the box into BUFF
        if (total_read >= box->size) {
            // wait untill there are messages to be read
            cond_wait(&box->condition, &box->mutex);
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
            fprintf(stderr, RESPONSE_INIT_ERR_MSG, SUBSCRIBER_OP_CODE);
            break;
        }

        // send response
        int r = subscriber_response_send(sub_fd, &sub_resp);
        if (r != 0) {
            // if EPIPE (closed pipe)
            if (r == -1) {
                // reassign handler to overwritte default SIGPIPE handler
                if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
                    PANIC(FATAL_SIGNAL_MSG);
                    exit(EXIT_FAILURE);
                };
            } else {
                fprintf(stderr, RESPONSE_SEND_ERR_MSG, SUBSCRIBER_OP_CODE);
            }
            break;
        }
        mutex_unlock(&box->mutex);
    }

    box->n_subscribers--;
    close(sub_fd);
    tfs_close(box_fd);

    mutex_unlock(&box->mutex);

    INFO(LOG_SUCCESS_SUB);
    return 0;
}

int handle_list(registration_request_t *req) {
    INFO(LOG_LIST_HANDLER);
    int manager_fd = open(req->pipe_name, O_WRONLY);
    if (manager_fd == -1) {
        fprintf(stderr, PIPE_OPEN_ERR_MSG, req->pipe_name);
        return -1;
    }

    // iterate through boxes and send them to the manager
    int j = 0;
    for (int i = 0; i < BOX_COUNT_MAX; i++) {
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
                fprintf(stderr, RESPONSE_INIT_ERR_MSG, LIST_MANAGER_OP);
                break;
            }

            if (list_manager_response_send(manager_fd, &resp) != 0) {
                fprintf(stderr, RESPONSE_SEND_ERR_MSG, LIST_MANAGER_OP);
                break;
            }
        }
    }

    close(manager_fd);

    INFO(LOG_SUCCESS_LIST);
    return 0;
}

/**
 * Return an mbroker's box with given string
 *
 */
box_t *get_box(char *name) {
    for (int i = 0; i < BOX_COUNT_MAX; ++i) {
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
