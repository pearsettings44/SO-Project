/**
 * Box structure functions
 */

#include "box.h"
#include "mbroker.h"
#include "operations.h"
#include "response.h"
#include "utils.h"
#include <string.h>

/**
 * Initializes a box structure managed by mbroker with given NAME.
 *
 * Returns 0 on success and -1 on failure
 */
int box_initialize(box_t *box, char *name) {
    memset(box, 0, sizeof(*box));

    // prepend / to string
    size_t len = strlen(name);
    char tmp[len + 1];
    tmp[0] = '/';
    tmp[1] = 0;
    strcat(tmp, name);

    if (strcpy(box->name, tmp) == NULL) {
        return -1;
    }

    // mark box as used
    box->alloc_state = USED;

    return 0;
}

/**
 * Creates a box in mbroker with given NAME if it doesn't already exist.
 * Writes result of operation to given RESP object.
 *
 * Returns 0 on success and -1 on failure
 */
int create_box(manager_response_t *resp, char *name) {
    // check if box exists
    if (get_box(name) != NULL) {
        manager_response_set_error_msg(resp, RESP_ERR_DUPLICATE_BOX);
        return -1;
    }

    box_t new_box;

    // initialize structure
    if (box_initialize(&new_box, name) != 0) {
        manager_response_set_error_msg(resp, RESP_ERR_INIT_BOX);
        return -1;
    }

    // create new file in TFS
    int fd = tfs_open(new_box.name, TFS_O_CREAT);
    if (fd == -1) {
        manager_response_set_error_msg(resp, RESP_ERR_CREATE_BOX);
        tfs_close(fd);
        return -1;
    }

    tfs_close(fd);

    box_t *boxes = get_mbroker_boxes_ref();
    // lock mbroker boxes internal storage structure
    mutex_lock(get_mbroker_boxes_lock());
    /**
     * Allocate space for the box, this should never fail because it relies on
     * TFS checks (having free space for a file), if the function gets here,
     * there is space for a box
     */
    for (int i = 0; i < BOX_COUNT_MAX; ++i) {
        if (boxes[i].alloc_state == NOT_USED) {
            boxes[i] = new_box;
            break;
        }
    }

    // unlock mbroker boxes internal storage structure
    mutex_unlock(get_mbroker_boxes_lock());

    return 0;
}

/**
 * Deletes a box in mbroker with given NAME. Box is removed from mbroker_boxes
 * if it exists.
 * Writes result of operation to given RESP
 *
 * Returns 0 on success and a -1 on failure
 */
int delete_box(manager_response_t *resp, char *name) {
    box_t *box = get_box(name);
    if (box == NULL) {
        manager_response_set_error_msg(resp, RESP_ERR_UNKNOWN_BOX);
        return -1;
    }

    // lock box mutex
    mutex_lock(&box->mutex);

    if (tfs_unlink(box->name) != 0) {
        manager_response_set_error_msg(resp, RESP_ERR_DELETE_BOX);
        mutex_unlock(&box->mutex);
        return -1;
    }

    box->alloc_state = NOT_USED;

    mutex_unlock(&box->mutex);
    // wake all threads that might be writting or reading from this box
    cond_broadcast(&box->condition);

    return 0;
}

/**
 * Writes MESSAGE to box opened with FD.
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
 * Reads a message from a box opened with FD and saves it to BUFFER.
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