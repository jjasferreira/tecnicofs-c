#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define THREAD_AMOUNT 4

int count = 0;

void* testfunc(){
    char *str = "ADS!";
    char *path = "/d2";

    int f;
    ssize_t r;

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);
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