#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *unlink(void *path) {
    int *r = malloc(sizeof(int));

    *r = tfs_unlink((char *)path);

    return (void *)r;
}

int main() {
    char *path = "/f1";
    pthread_t tid[3];
    int *ret1;
    int *ret2;

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_close(fd) != -1);

    /*
     * Attempt to unlink a file from two threads
     */
    pthread_create(&tid[0], NULL, unlink, (void *)path);
    pthread_create(&tid[1], NULL, unlink, (void *)path);

    pthread_join(tid[0], (void **)&ret1);
    pthread_join(tid[1], (void **)&ret2);

    assert((*ret1 == 0 && *ret2 == -1) || (*ret1 == -1 && *ret2 == 0));

    free(ret1);
    free(ret2);
    printf("Successful test.\n");
}
