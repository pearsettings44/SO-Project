#include "box.h"
#include "mbroker.h"
#include "operations.h"
#include "response.h"
#include <string.h>

/**
 * Initializes a box structure managed by mbroker.
 *
 * Receives box name as parameter
 */
int box_initialize(box_t *box, char *name) {
    memset(box, 0, sizeof(*box));

    // prepend / to string
    size_t len = strlen(name);
    char tmp[len + 1];
    tmp[0] = '/';
    tmp[1] = 0;
    strcat(tmp, name);
    // truncate if necessary
    tmp[len + 1] = 0;

    if (strcpy(box->name, tmp) == NULL) {
        return -1;
    }

    box->alloc_state = TAKEN;

    return 0;
}

/**
 * Creates a box in mbroker if it doesn't already exist.
 * Box is added to mbroker_boxes
 *
 * Writes result of operation to given response object
 *
 * Returns 0 if successful and -1 if failed
 */
int create_box(manager_response_t *resp, char *name) {
    // name = boxx
    box_t new_box;

    if (box_initialize(&new_box, name) != 0) {
        manager_response_set_error_msg(resp, "Couldn't initialize box\n");
        return -1;
    }

    // check if box already exists
    int fd = tfs_open(new_box.name, 0);
    if (fd != -1) {
        manager_response_set_error_msg(resp, "Box already exists\n");
        tfs_close(fd);
        return -1;
    }

    // create new file in TFS
    fd = tfs_open(new_box.name, TFS_O_CREAT);
    if (fd == -1) {
        manager_response_set_error_msg(resp, "Error creating box (TFS)\n");
        tfs_close(fd);
        return -1;
    }

    tfs_close(fd);

    box_t *boxes = get_boxes_list();
    allocation_state_t *bitmap = get_bitmap();
    /**
     * Allocate space for the box, this should never fail because it relies on
     * TFS checks (having free space for a file), if the function gets here,
     * there is space for a box
     */
    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        if (boxes[i].alloc_state == FREE) {
            boxes[i] = new_box;
            break;
        }
    }

    return 0;
}

/**
 * Deletes a box in mbroker with given name. Box is removed from mbroker_boxes
 * if it exists.
 *
 * Writes result of operation to given response object
 *
 * Returns 0 if successful and a negativa value if failed
 */
int delete_box(manager_response_t *resp, char *name) {
    box_t box;

    // in this case, this is only used to initialize the name i.e "box" ->
    // "/box"
    if (box_initialize(&box, name) != 0) {
        manager_response_set_error_msg(resp, "Couldn't initialize box\n");
        return -1;
    }

    int fd = tfs_open(box.name, 0);
    if (fd == -1) {
        manager_response_set_error_msg(resp, "Box doesn't exist\n");
        return -1;
    }
    tfs_close(fd);

    if (tfs_unlink(box.name) != 0) {
        manager_response_set_error_msg(resp, "Couldn't delete box (TFS)\n");
        return -1;
    }

    box_t *boxes = get_boxes_list();
    /**
     * Delete given block, this should never fail because it relies on
     * TFS checks (for a file existance), if the function gets here,
     * the block exists
     */
    for (int i = 0; i < MAX_BOX_COUNT; ++i) {
        if (boxes[i].alloc_state == TAKEN) {
            if (strcmp(box.name, boxes[i].name) == 0) {
                boxes[i].alloc_state = FREE;
                break;
            }
        }
    }

    return 0;
}

/**
 * Writes a message to box opened with fd
 */
ssize_t write_message(int fd, char *message) {
    size_t len = strlen(message);
    ssize_t ret = tfs_write(fd, message, len);
    // partial write or no write
    if (ret != len) {
        return -1;
    }

    // write \0 to sinalize end of message inside tfs file
    if (tfs_write(fd, "\0", 1) == -1) {
        return -1;
    }

    return ret;
}

/**
 * Reads a message from a box opened with fd, saves it to buffer
 */
ssize_t read_message(int fd, char *buffer) {
    int i = 0;
    do {
        ssize_t ret = tfs_read(fd, buffer + i, 1);
        // bad read
        if (ret != 1)
            return -1;
        i++;
    } while (buffer[i - 1] != '\0');

    return i;
}