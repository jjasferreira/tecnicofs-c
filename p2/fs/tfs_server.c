#include "operations.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

char* session[MAX_SESSIONS];
int fcli[MAX_SESSIONS];
int fserv;
//handle_commands
int handle_request(char *buffer);
int handle_tfs_mount(char *buffer);
int handle_tfs_unmount(char *buffer);
int handle_tfs_open(char *buffer);
int handle_tfs_close(char *buffer);
int handle_tfs_read(char *buffer);
int handle_tfs_write(char *buffer);
int handle_tfs_shutdown_after_all_closed(char *buffer);
//sessions
int try_session();
int open_session(char* client_pipe_path);
int close_session(int session_id);
//aux function
char* itoa(int val, int base);

int main(int argc, char **argv) {
    char buffer[MAX_REQUEST_SIZE];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    tfs_init(); //
    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) exit(1);
    if ((fserv = open(pipename, O_RDONLY)) < 0) exit(1);
    
    while (1) {
        if (read(fserv, buffer, MAX_REQUEST_SIZE) == -1) break;
        int r = handle_request(buffer);
        if ((r == -1) || (r == 1)) break;
    }
    if (close(fserv) == -1) return -1;
    unlink(pipename);
    return 0;
}

int handle_request(char* buffer) {
    int op_code;
    sscanf(buffer, "%d", &op_code);
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            if (handle_tfs_mount(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_UNMOUNT:
            if (handle_tfs_unmount(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_OPEN:
            if (handle_tfs_open(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_CLOSE:
            if (handle_tfs_close(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_WRITE:
            if (handle_tfs_write(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_READ:
            if (handle_tfs_read(buffer) == -1) return -1;
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            if (handle_tfs_shutdown_after_all_closed(buffer) == -1) return -1;
            break;
            
        default:
            return -1;
    }
    return 0;
}

int handle_tfs_mount(char *buffer) {
    int session_id;
    char *client_pipe_path;
    size_t r_size;
    
    r_size = MAX_SESSION_ID_LEN + 1;
    char result[r_size]; // session_id / -1
    client_pipe_path = (char*)malloc((MAX_PATH_NAME + 1) * sizeof(char));
    sscanf(buffer, "%s", client_pipe_path);
    session_id = open_session(client_pipe_path);
    strcpy(result, itoa(session_id, 10));
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (session_id == -1) return -1;
    return 0;
}

int handle_tfs_unmount(char *buffer) {
    int session_id, r;
    size_t r_size;

    r_size = 3;
    char result[r_size]; // 0 / -1
    sscanf(buffer, "%d", &session_id);
    r = close_session(session_id);
    strcpy(result, itoa(r, 10));
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (r == -1) return -1;
    return 0;
}

int handle_tfs_open(char *buffer) {
    int session_id, r, flags;
    char *name;
    size_t r_size;

    r_size = MAX_FHANDLE_LEN + 1;
    char result[r_size]; // file handle / -1
    name = (char*)malloc((MAX_FILE_NAME+1)*sizeof(char)); //TODO proteger estes mallocs com if -1 try again ou algo do género
    sscanf(buffer, "%d %s %d", &session_id, name, &flags);
    r = tfs_open(name, flags);
    strcpy(result, itoa(r, 10));
    free(name);
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (r == -1) return -1;
    return 0;
}

int handle_tfs_close(char *buffer) {
    int session_id, r;
    int fhandle;
    size_t r_size;

    r_size = 3;
    char result[r_size]; // 0 / -1
    sscanf(buffer, "%d %d", &session_id, &fhandle);
    r = tfs_close(fhandle);
    strcpy(result, itoa(r, 10));
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (r == -1) return -1;
    return 0;
}

int handle_tfs_write(char *buffer) {
    int session_id, r, fhandle;
    char *to_write;
    size_t r_size, len;

    r_size = 5; // len(BLOCK_SIZE) + 1
    char result[r_size]; // bytes read / -1
    sscanf(buffer, "%d %d %lu", &session_id, &fhandle, &len);
    to_write = (char*)malloc(len + 1);
    if (read(fserv, to_write, len) == -1) {
        free(to_write);
        return -1;
    }
    r = (int)tfs_write(fhandle, to_write, len);
    strcpy(result, itoa(r, 10));
    free(to_write);
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (r == -1) return -1;
    return 0;
}

int handle_tfs_read(char *buffer) {
    int session_id, r, fhandle;
    size_t len;

    sscanf(buffer, "%d %d %lu", &session_id, &fhandle, &len);
    char *result = (char*)malloc((len+1) * sizeof(char));
    r = (int)tfs_read(fhandle, result, len);
    if (write(fcli[session_id], result, len) == -1) {
        free(result);
        return -1;
    }
    free(result);
    if (r == -1) return -1;
    return 0;
}

int handle_tfs_shutdown_after_all_closed(char *buffer) {
    int session_id, r;
    size_t r_size;

    r_size = 3;
    char result[r_size]; // 0 / -1
    sscanf(buffer, "%d", &session_id);
    r = tfs_destroy_after_all_closed();
    strcpy(result, itoa(r, 10));
    if (write(fcli[session_id], result, r_size) == -1) return -1;
    if (r == 0) return 1;
    return 0;
}

int try_session() {
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (session[i] == NULL)
            return i;
    return -1;
}

int open_session(char* client_pipe_path) {
    int s_id = try_session();
    if (s_id != -1) {
        session[s_id] = client_pipe_path;
        if ((fcli[s_id] = open(client_pipe_path, O_WRONLY)) < 0) {
            free(client_pipe_path);
            return -1;
        }
        return s_id;
    }
    free(client_pipe_path);
    return -1;
}

int close_session(int session_id) {
    char* client_pipe_path = session[session_id];
    session[session_id] = NULL;
    if (close(fcli[session_id]) == -1) return -1;
    unlink(client_pipe_path);
    free(client_pipe_path);
    return 0;
}

char *itoa(int val, int base) {
    static char buf[32] = {0};
    int i = 30;
    for (; val && i && (val > -1); --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];
    return &buf[i + 1];
}