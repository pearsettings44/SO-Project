#ifndef BROKER_H
#define BROKER_H

#include "box.h"
#include "operations.h"
#include "requests.h"
#include "response.h"
#include "state.h"

/**
 * Configuration macros
*/
#define BOX_COUNT_MAX 1024
#define PIPE_PATHNAME_LENGTH 256

/**
 * Error messages
*/
#define PIPE_OPEN_ERR_MSG "ERROR: Failed opening pipe %s\n"
#define PIPE_PARTIAL_RW_ERR_MSG "ERROR: Partial read/write in pipe %s\n"
#define RESPONSE_INIT_ERR_MSG "ERROR: Failed initializing %d op_code response\n"
#define RESPONSE_SEND_ERR_MSG "ERROR: Failed sending %d op_code response to client\n"
#define BOX_INVALID_ERR_MSG "ERROR: Box %s doesn't exist\n"
#define BOX_HAS_PUBLISHER_ERR_MSG "ERROR: Another publisher is already writting to %s box\n"
#define BOX_TFS_ERR_MSG "ERROR: Couldn't access box %s data\n"

/**
 * Fatal error messages
*/
#define FATAL_SIGNAL_MSG "FATAL: Failed to overwrite default signal handler\n"
#define FATAL_TFS_INIT "FATAL: Failed to instanciate TFS\n"
#define FATAL_TFS_DESTROY "FATAL: Failed to destroy TFS\n"
#define FATAL_FIFO_CREATE "FATAL: Failed to create FIFO %s\n"
#define FATAL_PCQ_INIT "FATAL: Failed to initialize producer consumer queue\n"
#define FATAL_THREAD_LAUNCH "FATAL: Failed launching thread %d\n"
#define FATAL_THREAD_JOIN "FATAL: Failed waiting on thread %ld\n"

/**
 * Log messages
*/
#define LOG_REQUEST "Request received"
#define LOG_UNKNOWN_REQUEST "Ignoring unknown OP_CODE %d"
#define LOG_PUB_HANDLER "Handling publisher client"
#define LOG_SUB_HANDLER "Handling subscriber client"
#define LOG_MAN_HANDLER "Handling manager client"
#define LOG_LIST_HANDLER "Handling listing client"

int requests_handler(registration_request_t *);
int handle_publisher(registration_request_t *);
int handle_subscriber(registration_request_t *);
int handle_manager(registration_request_t *);
int handle_list(registration_request_t *);

box_t *get_box(char *name);
box_t *get_mbroker_boxes_ref();
pthread_mutex_t *get_mbroker_boxes_lock();

#endif