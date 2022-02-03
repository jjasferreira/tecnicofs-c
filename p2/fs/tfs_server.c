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
int handle_request(char* buffer);
char* itoa(int val, int base);

int main(int argc, char **argv) {
    char buffer[MAX_REQUEST_SIZE];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) exit(1);
    if ((fserv = open(pipename, O_RDONLY)) < 0) exit(1);
    
    while (1) {
        if (read(fserv, buffer, MAX_REQUEST_SIZE) == -1) {
            goto failure;
        }

        if (handle_request(buffer)) goto failure;
    }
    close (fserv);
    unlink(pipename);
    return 0;
    
    failure:
    close (fserv);
    unlink(pipename);
    printf("Deu bosta\n");
    exit(1);
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
        fcli[s_id] = open(client_pipe_path, O_WRONLY);
        if (fcli[s_id] == -1) {
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
    if (close(fcli[session_id])) return -1;
    unlink(client_pipe_path);
    free(client_pipe_path);
    return 0;
}

int handle_request(char* buffer) {
    int op_code, session_id;
    char *client_pipe_path, *result, *name, *fhandle, *to_write;
    sscanf(buffer, "%d", &op_code);
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            client_pipe_path = (char*)malloc((MAX_PATH_NAME+1)*sizeof(char));
            sscanf(buffer, "%s", client_pipe_path);
            session_id = open_session(client_pipe_path);
            char* id_char = itoa(session_id, 10);
            //write result
            if (write(fcli[session_id], id_char, strlen(id_char)) == -1) return -1;
            break;

        case TFS_OP_CODE_UNMOUNT:
            sscanf(buffer, "%d", &session_id);
            result = itoa(close_session(session_id), 10);
            //write result
            if (write(fcli[session_id], result, sizeof(char))) return -1;
            break;

        case TFS_OP_CODE_OPEN:
            int flags;
            name = (char*)malloc((MAX_FILE_NAME+1)*sizeof(char)); //TODO proteger estes mallocs com if -1 try again ou algo do género
            sscanf(buffer, "%d %s %d", &session_id, name, &flags);
            fhandle = itoa(tfs_open(name, flags), 10);
            free(name);
            //write fhandle
            write(fcli[session_id], fhandle, strlen(fhandle));
            break;

        case TFS_OP_CODE_CLOSE:
            int fildes;
            sscanf(buffer, "%d %d", &session_id, &fildes);
            result = itoa(tfs_close(fildes), 10);
            //write result
            write(fcli[session_id], result, strlen(result));
            break;

        case TFS_OP_CODE_WRITE:
            size_t len;
            to_write = (char*)malloc(MAX_REQUEST_SIZE + 1);
            sscanf(buffer, "%d %d %lu", &session_id, &fildes, &len);
            if (read(fserv, buffer, sizeof(buffer))) { 
                free(to_write);
                return -1;
            }
            sscanf(buffer, "%s", to_write);
            result = itoa((int)tfs_write(fildes, to_write, len), 10);
            free(to_write);
            //write result
            write(fcli[session_id], result, strlen(result));
            break;

        case TFS_OP_CODE_READ:
            sscanf(buffer, "%d %d %lu", &session_id, &fildes, &len);
            result = itoa((int)tfs_read(fildes, buffer, len), 10);
            write(fcli[session_id], buffer, len);
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            sscanf(buffer, "%d", &session_id);
            int destroyed = tfs_destroy_after_all_closed();
            result = (char*)malloc(9*sizeof(char));
            strcat(result, destroyed + "");
            write(fcli[session_id], result, strlen(result));
            free(result);
            if (destroyed == 0)
                return 1;
            break;
            
        default:
            return -1;
    }
    return 0;
}

char* itoa(int val, int base) {
    static char buf[32] = {0};
    int i = 30;
    for (; val && i && (val > -1); --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];
    return &buf[i + 1];
}