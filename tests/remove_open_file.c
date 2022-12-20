#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char *path = "/f1";

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    // Unlink file
    assert(tfs_unlink(path) == -1);

    printf("Successful test.\n");
}
