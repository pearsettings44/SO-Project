#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char const *file_contents[] = {"AAA!", "BBB!", "CCC!", "DDD!",
                               "EEE!", "FFF!", "HHH!", "GGG!"};
char const *target_path[] = {"/f1", "/f2", "/f3", "/f4",
                             "/f5", "/f6", "/f7", "/f8"};

void assert_contents_ok() {
    char buffer[10];
    for (int i = 0; i < 8; ++i) {
        int fd = tfs_open(target_path[i], 0);
        assert(tfs_read(fd, buffer, 5) == 5);
        assert(memcmp(buffer, file_contents[i], 5) == 0);
        assert(tfs_close(fd) != -1);
    }
}

/*
 * Create multiple files and write to them
 */
void *thread_fnc(void *i) {
    int fd = tfs_open(target_path[*(int *)i], TFS_O_CREAT);
    assert(fd != -1);

    tfs_write(fd, file_contents[*(int *)i], 5);

    return NULL;
}

// meant to be tested with fsanitize=thread
int main() {
    pthread_t tid[8];
    int *indexs[8];
    assert(tfs_init(NULL) != -1);

    /*
     * Open and write to different files
     */
    for (int i = 0; i < 8; ++i) {
        indexs[i] = malloc(sizeof(int));
        *indexs[i] = i;
        if (pthread_create(&tid[i], NULL, thread_fnc, indexs[i]) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 8; ++i) {
        pthread_join(tid[i], NULL);
    }

    assert_contents_ok();

    printf("Successful test.\n");

    return 0;
}
