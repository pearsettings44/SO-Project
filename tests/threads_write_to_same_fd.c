#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t const file_contents[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
char const target_path1[] = "/f1";

void *assert_write_ok(int fd) {

    // should return 0 because file is full
    assert(tfs_write(fd, file_contents, sizeof(file_contents)) == 0);

    return NULL;
}

void *write_contents(void *fd) {

    assert(tfs_write(*(int *)fd, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    return NULL;
}

int main() {
    pthread_t tid[16];
    assert(tfs_init(NULL) != -1);
    int *fd = malloc(sizeof(int));

    *fd = tfs_open(target_path1, TFS_O_CREAT);

    /*
     * Use the same open_file_entry to write 64 bytes to a file (\000 included)
     * At the end, the file should be full, so another write operation should
     * return 0
     */
    for (int i = 0; i < 16; ++i) {
        if (pthread_create(&tid[i], NULL, write_contents, (void *)fd) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 16; ++i) {
        pthread_join(tid[i], NULL);
    }

    assert_write_ok(*fd);
    assert(tfs_close(*fd) != -1);

    printf("Successful test.\n");

    return 0;
}
