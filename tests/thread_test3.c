#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define COUNT 40
#define SIZE 250
#define THREAD_AMOUNT 4

int count = 0;

void* testfunc(){
    char *path = "/d3";

    char input[SIZE]; 
    memset(input, 'A', SIZE);

    char output [SIZE];

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, SIZE) == SIZE);
    }
    assert(tfs_close(fd) != -1);

    fd = tfs_open(path, 0);
    assert(fd != -1 );

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert (memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(fd) != -1);
    count++;
    return NULL;
}

int main() {
    pthread_t tid[THREAD_AMOUNT];

    assert(tfs_init() != -1);

    for (int i = 0; i < THREAD_AMOUNT; ++i) {
        if (pthread_create(&tid[i], NULL, testfunc, NULL) != 0)
            exit(EXIT_FAILURE);
    }
    for (int i = 0; i < THREAD_AMOUNT; ++i) 
        pthread_join(tid[i], NULL);
    tfs_destroy();
    assert(count==THREAD_AMOUNT);
    printf("Successful test\n");
    exit(EXIT_SUCCESS);
}