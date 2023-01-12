#ifndef BROKER_H
#define BROKER_H

#include "box.h"
#include "operations.h"
#include "requests.h"
#include "response.h"
#include "state.h"

#define MAX_BOX_COUNT 1024
#define PUB_PIPE_PATHNAME 256

int handle_publisher(registration_request_t *);
int handle_subscriber(registration_request_t *);
int handle_manager(registration_request_t *);
int handle_list(registration_request_t *);

box_t *get_box(char *name);
// we did this because w had no real time to fix the linking stuff
box_t *get_boxes_list();
allocation_state_t *get_bitmap();

#endif