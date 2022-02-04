#include "operations.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>

pthread_t tasks[MAX_SESSIONS];
pthread_mutex_t lock;
pthread_cond_t mayProduce[MAX_SESSIONS], mayReceive[MAX_SESSIONS];

typedef struct {
    int op_code = NULL;
    int session_id = NULL;
    char* txt_info = NULL;
    int fhandle = NULL;
    int flags = NULL;
    size_t len = NULL;
} parsed_command;

int session_count;

char* session[MAX_SESSIONS];
int fcli[MAX_SESSIONS]
int fserv;
// Haxndle Commands
int handle_request(char *buffer);
int handle_tfs_mount(char *buffer);
int handle_tfs_unmount(char *buffer);
int handle_tfs_open(char *buffer);
int handle_tfs_close(char *buffer);
int handle_tfs_read(char *buffer);
int handle_tfs_write(char *buffer);
int handle_tfs_shutdown_after_all_closed(char *buffer);
// Auxiliary Functions
int try_session();
int open_session(char* client_pipe_path);
int close_session(int session_id);

int main(int argc, char **argv) {
    char buffer[MAX_REQUEST_SIZE];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        exit(1);
    }
    

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    tfs_init(); //
    unlink(pipename);

    if (pthread_mutex_init(&lock, 0) != 0)
        exit(1);
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (pthread_cond_init(&mayProduce[i], NULL) != 0)
            exit(1);
        if (pthread_cond_init(&mayReceive[i], NULL) != 0)
            exit(1);
    }
    
    if (mkfifo(pipename, 0777) < 0) exit(1);
    if ((fserv = open(pipename, O_RDONLY)) < 0) exit(1);
    
    while (1) {
        if (read(fserv, buffer, MAX_REQUEST_SIZE) < 0) break;
        int r = handle_request(buffer);
        if ((r < 0) || (r == 1)) break;
    }
    if (close(fserv) < 0) exit(1);
    unlink(pipename);
    return 0;
}

parsed_command parse_command(char* buffer) {
    parsed_command* command = (parsed_command*) malloc(sizeof(parsed_command));
    int op_code
    sscanf(buffer, "%d ", &op_code);
    command->op_code = op_code;
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            command->txt_info = (char*)malloc(MAX_PATH_NAME + 1);
            sscanf(buffer, "%s", command->txt_info);
            break;
        case TFS_OP_CODE_UNMOUNT:
            sscanf(buffer, "%d", &(command->session_id));
            break;
        case TFS_OP_CODE_OPEN:
            command->txt_info = (char*)malloc(MAX_FILE_NAME + 1);
            sscanf(buffer, "%d %s %d", &(command->session_id), command->txt_info, &(command->flags));
            break;
        case TFS_OP_CODE_CLOSE:
            sscanf(buffer, "%d %d", &(command->session_id), &(command->fhandle));
            break;
        case TFS_OP_CODE_WRITE:
            sscanf(buffer, "%d %d %lu", &(command->session_id), &(command->fhandle), &(command->len));
            break;
        case TFS_OP_CODE_READ:
            sscanf(buffer, "%d %d %lu", &(command->session_id), &(command->fhandle), &(command->len));
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            sscanf(buffer, "%d", &(command->session_id));
            break;   
        default:
            return -1;
    }
    return 0;
} 


int handle_request(char* buffer) {
    int op_code;
    sscanf(buffer, "%d ", &op_code);
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            if (handle_tfs_mount(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_UNMOUNT:
            if (handle_tfs_unmount(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_OPEN:
            if (handle_tfs_open(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_CLOSE:
            if (handle_tfs_close(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_WRITE:
            if (handle_tfs_write(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_READ:
            if (handle_tfs_read(buffer) < 0) return -1;
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            if (handle_tfs_shutdown_after_all_closed(buffer) < 0) return -1;
            break;
            
        default:
            return -1;
    }
    return 0;
}

int handle_tfs_mount(char *buffer) {
    int result; // session_id || -1
    char *client_pipe_path = (char*)malloc(MAX_PATH_NAME + 1);

    sscanf(buffer, "%s", client_pipe_path);
    result = open_session(client_pipe_path);
    if (write(fcli[result], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_unmount(char *buffer) {
    int session_id, result; // 0 || -1

    sscanf(buffer, "%d", &session_id);
    result = close_session(session_id);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_open(char *buffer) {
    int session_id, flags, result; // fhandle || -1
    char *name = (char*)malloc(MAX_FILE_NAME + 1); //TODO proteger estes mallocs com if -1 try again?

    sscanf(buffer, "%d %s %d", &session_id, name, &flags);
    result = tfs_open(name, flags);
    free(name);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_close(char *buffer) {
    int session_id, fhandle, result; // 0 || -1

    sscanf(buffer, "%d %d", &session_id, &fhandle);
    result = tfs_close(fhandle);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_write(char *buffer) {
    int session_id, fhandle, result; // bytes || -1
    char *to_write;
    size_t len;

    sscanf(buffer, "%d %d %lu", &session_id, &fhandle, &len);
    to_write = (char*)malloc(len + 1);
    if (read(fserv, to_write, len) < 0) {
        free(to_write);
        return -1;
    }
    result = (int)tfs_write(fhandle, to_write, len);
    free(to_write);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_read(char *buffer) {
    int session_id, fhandle, result; // bytes || -1
    char *to_read;
    size_t len;

    sscanf(buffer, "%d %d %lu", &session_id, &fhandle, &len);
    to_read = (char*)malloc(len + 1);
    result = (int)tfs_read(fhandle, to_read, len);
    if (write(fcli[session_id], to_read, len) < 0) {
        free(to_read);
        return -1;
    }
    free(to_read);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    return 0;
}

int handle_tfs_shutdown_after_all_closed(char *buffer) {
    int session_id, result; // 0 || -1

    sscanf(buffer, "%d", &session_id);
    result = tfs_destroy_after_all_closed();
    if (write(fcli[session_id], &result, sizeof(int)) < 0) return -1;
    if (result == 0) return 1;
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
        session_count++;
        return s_id;
    }
    free(client_pipe_path);
    return -1;
}

int close_session(int session_id) {
    char* client_pipe_path = session[session_id];
    session[session_id] = NULL;
    if (close(fcli[session_id]) < 0) return -1;
    unlink(client_pipe_path);
    free(client_pipe_path);
    session_count--;
    return 0;
}