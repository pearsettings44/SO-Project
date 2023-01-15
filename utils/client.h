#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

#define PIPE_CREATE_ERR_MSG "ERROR: Failed creating pipe %s\n"
#define PIPE_DELETE_ERR_MSG "ERROR: Failed deleting pipe %s\n"
#define PIPE_OPEN_ERR_MSG "ERROR: Failed opening pipe %s\n"
#define PIPE_PARTIAL_RW_ERR_MSG "ERROR: Partial read/write in pipe %s\n"
#define INVALID_OP_ERR_MSG                                                     \
    "ERROR: Pipe closed by mbroker, operation was invalid\n"
#define SIGINT_MSG "SIGINT received, closing session\n"
#define SIGNAL_ERR_MSG "ERROR: Failed to assign new handler to %s\n"

int connect_to_mbroker(char *broker_pipe, char *pipe_name, char *box_name,
                       uint8_t op_code, int comm_flags);
void exit_failure(int fd, char *pipe_name);

#endif