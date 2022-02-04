#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

int session_id;
int fcli, fserv;
size_t c_size, r_size;
char* pipename;

int num_digits(int n);

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    pipename = (char*)malloc(strlen(client_pipe_path));
    strcpy(pipename, client_pipe_path);
    unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0777) < 0) return -1; // TODO o professor não tinha dito que devíamos tentar outra vez?

    c_size = 2 + MAX_SESSION_ID_LEN + 1 + MAX_PATH_NAME;
    char command[c_size];
    int result; // session_id || -1

    sprintf(command, "%d %s", TFS_OP_CODE_MOUNT, client_pipe_path);
    if ((fserv = open(server_pipe_path, O_WRONLY)) < 0) return -1;
    if (write(fserv, command, c_size) < 0) return -1;
    if ((fcli = open(client_pipe_path, O_RDONLY)) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    if ((session_id = result) < 0) return -1;
    return 0;
}

int tfs_unmount() {
    c_size = 2 + MAX_SESSION_ID_LEN + 1;
    char command[c_size];
    int result; // 0 || -1

    sprintf(command, "%d %d", TFS_OP_CODE_UNMOUNT, session_id);
    if (write(fserv, command, c_size) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    if (result < 0) return -1;

    if (close(fcli) < 0) return -1;
    if (close(fserv) < 0) return -1;
    unlink(pipename);
    free(pipename);
    return 0;
}

int tfs_open(char const *name, int flags) {
    c_size = 2 + MAX_SESSION_ID_LEN + 1 + MAX_FILE_NAME + 4;
    char command[c_size];
    int result; // fhandle || -1

    sprintf(command, "%d %d %s %d", TFS_OP_CODE_OPEN, session_id, name, flags);
    if (write(fserv, command, c_size) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    return result;
}

int tfs_close(int fhandle) {
    c_size = 2 + MAX_SESSION_ID_LEN + 1 + MAX_FHANDLE_LEN + 1;
    char command[c_size];
    int result; // 0 || -1

    sprintf(command, "%d %d %d", TFS_OP_CODE_CLOSE, session_id, fhandle);
    if (write(fserv, command, c_size) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    return result;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    c_size = (size_t)(2 + MAX_SESSION_ID_LEN + 1 + MAX_FHANDLE_LEN + 1 + num_digits((int)len) + 1);
    char* command = (char*)malloc(c_size);
    int result; // bytes || -1

    sprintf(command, "%d %d %d %lu", TFS_OP_CODE_WRITE, session_id, fhandle, len);
    if (write(fserv, command, c_size) < 0) {
        free(command);
        return -1;
    }
    free(command);
    if (write(fserv, buffer, len) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    return result;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    c_size = (size_t)(2 + MAX_SESSION_ID_LEN + 1 + MAX_FHANDLE_LEN + 1 + num_digits((int)len) + 1);
    char* command = (char*)malloc(c_size);
    int result; // bytes || -1

    sprintf(command, "%d %d %d %lu", TFS_OP_CODE_READ, session_id, fhandle, len);
    if (write(fserv, command, c_size) < 0) {
        free(command);
        return -1;
    }
    free(command);
    if (read(fcli, buffer, len) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    return result;
}

int tfs_shutdown_after_all_closed() {
    c_size = 2 + MAX_SESSION_ID_LEN + 1;
    char command[c_size];
    int result; // 0 || -1

    sprintf(command, "%d %d", TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED, session_id);
    if (write(fserv, command, c_size) < 0) return -1;
    if (read(fcli, &result, sizeof(int)) < 0) return -1;
    return result;
}

int num_digits(int n) {
    int count = 0;
    while (n != 0) {  
       n = n/10;
       count++;
    }
    return count;
}