#include "tecnicofs_client_api.h"
#include <fcntl.h>

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    // TODO
    int fcli;
    unlink(client_pipe_path);

    if (mkfifo(client_pipe_path, 0777) < 0)
        exit(1);
    if ((fcli = open(client_pipe_path, O_WRONLY)) < 0)
        exit(1);
    
    lose (fcli);
    unlink(client_pipe_path);
    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    // write (fserv, buf, TAMMSG);
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
