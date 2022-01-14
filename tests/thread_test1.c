#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void* testfunc(){
    printf("GG you're awesome\n");
    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    int f;
    ssize_t r;

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);
    return NULL;
}

int main(int argc, char** argv) {
    pthread_t tid[4];

    assert(tfs_init() != -1);

    for (int i; i < 4; ++i) {
        if (pthread_create (&tid[i], NULL, testfunc, NULL) == 0)
            exit(EXIT_FAILURE);
    }
    for (int i; i < 4; ++i) 
        pthread_join(tid[i], NULL);
    tfs_destroy();
    exit(EXIT_SUCCESS);
}