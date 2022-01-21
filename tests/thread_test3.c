#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define COUNT 40
#define SIZE 250
#define NUM_THREADS 4
    
int count = 0;

void* testfunc() {

    char *path = "/e3";
    char input[SIZE], output[SIZE];
    memset(input, 'A', SIZE);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    for (int i = 0; i < COUNT; i++)
        assert(tfs_write(f, input, SIZE) == SIZE);

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(f, output, SIZE) == SIZE);
        assert(memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(f) != -1);

    count++;
    return NULL;
}

int main() {

    pthread_t tid[NUM_THREADS];

    assert(tfs_init() != -1);

    for (int i = 0; i < NUM_THREADS; ++i)
        assert(pthread_create(&tid[i], NULL, testfunc, NULL) == 0);

    for (int i = 0; i < NUM_THREADS; ++i) 
        assert(pthread_join(tid[i], NULL) == 0);
    
    assert(tfs_destroy() != -1);

    assert(count == NUM_THREADS);

    printf("Successful test.\n");
    return 0;
}