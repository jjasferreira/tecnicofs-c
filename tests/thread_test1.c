#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define NUM_THREADS 4

int count = 0;

void* testfunc() {

    char *str = "ABC!";
    char *path = "/f1";

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    ssize_t r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    count++;
}

int main() {

    pthread_t tid[NUM_THREADS];

    assert(tfs_init() != -1);

    for (int i = 0; i < NUM_THREADS; i++)
        assert(pthread_create(&tid[i], NULL, testfunc, NULL) == 0);

    for (int i = 0; i < NUM_THREADS; i++) 
        assert(pthread_join(tid[i], NULL) == 0);
    
    assert(tfs_destroy() != -1);

    assert(count == NUM_THREADS);

    printf("Successful test\n");
    return 0;
}