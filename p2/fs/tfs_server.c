#include "operations.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

pthread_t tasks[MAX_SESSIONS];
pthread_mutex_t locks[MAX_SESSIONS];
pthread_cond_t mayWork[MAX_SESSIONS], maySend[MAX_SESSIONS];

typedef struct {
    int op_code;
    int session_id;
    char *txt_info; 
    int fhandle;
    int flags;
    size_t len;
} parsed_command;

parsed_command *command_buffer[MAX_SESSIONS];
int busy[MAX_SESSIONS];

int numbers[MAX_SESSIONS];
char *session[MAX_SESSIONS];
int fcli[MAX_SESSIONS];
int fserv;
char *pipename;

parsed_command* parse_command(char* buffer);

// Handle Commands
void *handle_request(void *session_id);
int handle_tfs_mount(parsed_command* command);
int handle_tfs_unmount(parsed_command* command);
int handle_tfs_open(parsed_command* command);
int handle_tfs_close(parsed_command* command);
int handle_tfs_read(parsed_command* command);
int handle_tfs_write(parsed_command* command);
int handle_tfs_shutdown_after_all_closed(parsed_command* command);

// Auxiliary Functions
int init_server();
int destroy_server();
int try_session();
int open_session(char* client_pipe_path);
int close_session(int session_id);

int main(int argc, char **argv) {

    char buffer[MAX_REQUEST_SIZE];
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        exit(1);
    }
    pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    tfs_init();
    signal(SIGPIPE, SIG_IGN);
    unlink(pipename);

    if (init_server()) exit(1);
    if (mkfifo(pipename, 0777) < 0) exit(1);
    if ((fserv = open(pipename, O_RDONLY)) < 0) exit(1);
    
    parsed_command *command;
    int session_id;
    while (1) {
        // Reads request, parses command
        if (read(fserv, buffer, MAX_REQUEST_SIZE) < 0) break;
        command = parse_command(buffer);
        /* case TFS_OP_CODE_MOUNT: // TODO should this be multithreaded?
                if (handle_tfs_mount(command) < 0) return NULL;
                break; */
        if (command == NULL) break;
        if (command->op_code == TFS_OP_CODE_MOUNT) {
            handle_tfs_mount(command);
            continue;
        }
        session_id = command->session_id;

        pthread_mutex_lock(&(locks[session_id]));

        while (busy[session_id])
            pthread_cond_wait(&maySend[session_id], &locks[session_id]);
        command_buffer[session_id] = command;
        busy[session_id] = 1;

        pthread_cond_signal(&mayWork[session_id]);
        pthread_mutex_unlock(&locks[session_id]);
    }
    if (close(fserv) < 0) exit(1);
    if (destroy_server()) exit(1);
    unlink(pipename);
    return 0;
}

parsed_command *parse_command(char* buffer) {
    parsed_command* command = (parsed_command*)malloc(sizeof(parsed_command));
    int op_code;
    sscanf(buffer, "%d ", &op_code);
    command->op_code = op_code;
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            command->txt_info = (char*)malloc(MAX_PATH_NAME + 1); // pipename
            sscanf(buffer, "%s", command->txt_info);
            break;
        case TFS_OP_CODE_UNMOUNT:
            sscanf(buffer, "%d", &(command->session_id));
            break;
        case TFS_OP_CODE_OPEN:
            command->txt_info = (char*)malloc(MAX_FILE_NAME + 1); // filename
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
            return NULL;
    }
    return command;
} 

void *handle_request(void* s_id) {
    while(1) {
        int session_id = *((int*)s_id);
        pthread_mutex_lock(&locks[session_id]);
        while (!busy[session_id])
            pthread_cond_wait(&mayWork[session_id], &locks[session_id]);
        parsed_command* command = command_buffer[session_id];
        int op_code = command->op_code;
        switch (op_code) {
            case TFS_OP_CODE_UNMOUNT:
                if (handle_tfs_unmount(command) < 0) return NULL;
                break;

            case TFS_OP_CODE_OPEN:
                if (handle_tfs_open(command) < 0) return NULL;
                break;

            case TFS_OP_CODE_CLOSE:
                if (handle_tfs_close(command) < 0) return NULL;
                break;

            case TFS_OP_CODE_WRITE:
                if (handle_tfs_write(command) < 0) return NULL;
                break;

            case TFS_OP_CODE_READ:
                if (handle_tfs_read(command) < 0) return NULL;
                break;

            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                if (handle_tfs_shutdown_after_all_closed(command) < 0) return NULL;
                break;
                
            default:
                return NULL;
        }
        command_buffer[session_id] = NULL;
        pthread_cond_signal(&maySend[session_id]);
        pthread_mutex_unlock(&locks[session_id]);
    } 
    return NULL;
}

int handle_tfs_mount(parsed_command* command) {
    int result; // session_id || -1
    result = open_session(command->txt_info/* client_pipe_path */);
    if (write(fcli[result], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    free(command);
    return 0;
}

int handle_tfs_unmount(parsed_command* command) {
    int result, session_id = command->session_id; // 0 || -1
    result = close_session(session_id);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    free(command);
    return 0;
}

int handle_tfs_open(parsed_command* command) {
    int session_id = command->session_id, flags = command->flags, result; // fhandle || -1
    result = tfs_open(command->txt_info, flags);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    free(command->txt_info);
    free(command);
    return 0;
}

int handle_tfs_close(parsed_command* command) {
    int session_id = command->session_id, fhandle = command->fhandle, result; // 0 || -1
    result = tfs_close(fhandle);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    free(command);
    return 0;
}

int handle_tfs_write(parsed_command* command) {
    int session_id = command->session_id, fhandle = command->fhandle, result; // bytes || -1
    char *to_write;
    size_t len = command->len;
    to_write = (char*)malloc(len + 1);
    if (read(fserv, to_write, len) < 0) {
        free(to_write);
        return -1;
    }
    result = (int)tfs_write(fhandle, to_write, len);
    free(to_write);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    free(command);
    return 0;
}

int handle_tfs_read(parsed_command* command) {
    int session_id = command->session_id, fhandle = command->fhandle, result; // bytes || -1
    char *to_read;
    size_t len = command->len;
    free(command);
    to_read = (char*)malloc(len + 1);
    result = (int)tfs_read(fhandle, to_read, len);
    if (write(fcli[session_id], to_read, len) < 0) {
        free(to_read);
        return -1;
    }
    free(to_read);
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        return -1;
    }
    return 0;
}

int handle_tfs_shutdown_after_all_closed(parsed_command* command) {
    int session_id = command->session_id, result; // 0 || -1

    result = tfs_destroy_after_all_closed();
    if (write(fcli[session_id], &result, sizeof(int)) < 0) {
        free(command);
        return -1;
    }
    if (result == 0) return 1;
    free(command);
    return 0;
}

int init_server() {
    int i;
    for (i = 0; i < MAX_SESSIONS; i++) {
        numbers[i] = i;
    }
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (pthread_create(&tasks[i], NULL, handle_request, (void*)&(numbers[i]))) return -1;
        if (pthread_mutex_init(&locks[i], NULL)) return -1;
        if (pthread_cond_init(&mayWork[i], NULL)) return -1;
        if (pthread_cond_init(&maySend[i], NULL)) return -1;
        command_buffer[i] = NULL;
        busy[i] = 0;
    }
    return 0;
}

int destroy_server() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (pthread_join(tasks[i], NULL)) return -1;
        if (pthread_mutex_destroy(&locks[i])) return -1;
        if (pthread_cond_destroy(&mayWork[i])) return -1;
        if (pthread_cond_destroy(&maySend[i])) return -1;
    }
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
    if (close(fcli[session_id]) < 0) return -1;
    unlink(client_pipe_path);
    free(client_pipe_path);
    return 0;
}