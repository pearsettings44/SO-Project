#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

uint8_t const file_contents[] = "AAA!";
char const target_path1[] = "/f1";

void *assert_contents_ok(void *path) {
    int f = tfs_open((char *)path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);

    return NULL;
}

void *write_contents(void *path) {
    int f = tfs_open((char *)path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);

    return NULL;
}

void *read_contents(void *path) {
    int f = tfs_open((char *)path, 0);
    assert(f != -1);

    char BUFFER[5];
    ssize_t r = tfs_read(f, BUFFER, sizeof(BUFFER));

    assert(r == 5 || r == 0);

    return NULL;
}

// meant to be tested with fsanitize=thread
int main() {
    pthread_t tid[2];
    assert(tfs_init(NULL) != -1);

    int fd = tfs_open(target_path1, TFS_O_CREAT);

    assert(tfs_close(fd) != -1);

    pthread_create(&tid[0], NULL, write_contents, (void *)target_path1);
    pthread_create(&tid[1], NULL, read_contents, (void *)target_path1);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    
    assert_contents_ok((void *)target_path1);

    printf("Successful test.\n");
    
    return 0;
}
