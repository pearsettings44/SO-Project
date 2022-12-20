#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

void *create_file(void *args) {
    // trick compiler
    (void)args;

    int *f = malloc(sizeof(int));
    *f = tfs_open("/f1", TFS_O_CREAT);
    assert(*f != -1);

    return f;
}

void assert_read_ok() {
    int fd = tfs_open("/f1", 0);
    assert(fd != -1);

    char buffer[16];
    assert(tfs_read(fd, buffer, sizeof(buffer)) == 15);

    assert(tfs_close(fd) != -1);
}

int main() {
    int *fd[15];
    pthread_t tid[15];

    assert(tfs_init(NULL) != -1);

    /*
     * Open the same file with multiple threads and store all file descriptors
     */
    for (int i = 0; i < 15; ++i) {
        if (pthread_create(&tid[i], NULL, create_file, NULL) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 15; ++i) {
        pthread_join(tid[i], (void **)&fd[i]);
    }

    /*
     * Write into the EOF a character for each file descriptor
     * After checking the contents, there should be 15 characters, one per 
     * thread
     */
    char buffer[16];
    char content = 'A';
    // write a character from each file descriptor
    for (int i = 0; i < 15; ++i) {
        // read first so following tfs_write is appended to EOF
        assert(tfs_read(*fd[i], &buffer, 16) != -1);
        assert(tfs_write(*fd[i], &content, 1) == 1);

        assert(tfs_close(*fd[i]) != -1);
        free(fd[i]);
    }

    assert_read_ok();
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}