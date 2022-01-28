#include "tecnicofs_client_api.h"
#include <fcntl.h>

int session_id;
int fcli, fserv;
char op_code;
char* pipename;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    int fcli;
    pipename = client_pipe_path;
    unlink(client_pipe_path);
    op_code = TFS_OP_CODE_MOUNT + '0';
    char buffer[MAX_PATH_NAME + 3] = {op_code, ' '};
    strcat(buffer, client_pipe_path);

    if (mkfifo(client_pipe_path, 0777) < 0) return -1;

    if ((fcli = open(client_pipe_path, O_RDONLY)) < 0) return -1;
    if ((fserv = open(server_pipe_path, O_WRONLY)) < 0) return -1;

    if (write(fserv, &buffer, MAX_PATH_NAME + 3)) return -1;
    if (read(fcli, &buffer, sizeof(char))) return -1;

    sscanf(buffer, "%d", session_id);
    close (fcli);
    if (session_id == -1)
        return -1;
    return 0;
}

int tfs_unmount() {

    op_code = TFS_OP_CODE_UNMOUNT + '0';
    char buffer[sizeof(MAX_SESSIONS) + 3] = {op_code, ' '};
    strcat(buffer, session_id);
    if (write(fserv, &buffer, sizeof(MAX_SESSIONS) + 3)) return -1;
    close(fcli);
    close(fserv);
    unlink(pipename);
    return 0;
}

int tfs_open(char const *name, int flags) {
    // TODO
    // write "op_code | session_id | char[40]name | flags"
    return -1;
}

int tfs_close(int fhandle) {
    // TODO
    // write "op_code | session_id | fhandle"
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    // TODO
    // write "op_code | session_id | fhandle | size | char[len] buffer"
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    // TODO
    // write "op_code | session_id | fhandle | len"
    // read "number bytes read | char[number bytes read] conteudo"
    return -1;
}

int tfs_shutdown_after_all_closed() {
    op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED + '0';
    char buffer[4*sizeof(int)] = {op_code, ' '};
    strcat(buffer, session_id);
    if (write(fserv, &buffer, 4*sizeof(int))) return -1;
    return -1;
}
