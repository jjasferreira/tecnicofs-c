#include "operations.h"
#include <fcntl.h>

char* session[MAX_SESSIONS];
int fcli[MAX_SESSIONS];

char buffer[MAX_REQUEST_SIZE];
char result[MAX_REQUEST_SIZE];
int fserv;

int main(int argc, char **argv) {

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
        if (read(fserv, buffer, sizeof(buffer))) break;
        if (handle_request(buffer, result)) break;
        if (write(fcli, result, size_of(result))) break;
    }
    close (fserv);
    unlink(pipename);
    return 0;
}

int open_session(char const* client_pipe_path) {

    int s_id = try_session();
    if (s_id != -1)
        session[s_id] = client_pipe_path;
        if (fcli[s_id] = open(client_pipe_path, O_WRONLY) < 0) return -1;
        return s_id;
    return -1;
}
    
int try_session() {

    for (int i = 0; i < MAX_SESSIONS; i++)
        if (session[i] == NULL)
            return i;
    return -1;
}

int close_session(session_id) {

    char* client_pipe_path = session[session_id];
    session[session_id] = NULL;
    close(fcli[session_id]);
    unlink(client_pipe_path);
    return 0;
}

int handle_request(char* buffer, char* result) {
    int op_code, session_id;
    char* client_pipe_path;
    sscanf(buffer, "%d", &op_code);
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            sscanf(buffer, "%s", client_pipe_path);
            session_id = open_session(client_pipe_path);
            strcat(result, session_id);
            break;
        case TFS_OP_CODE_UNMOUNT:
            sscanf(buffer, "%d", &session_id);
            close_session(session_id);
            // write
            break;
        case TFS_OP_CODE_OPEN:
            //TODO
            break;
        case TFS_OP_CODE_CLOSE:
            //TODO
            break;
        case TFS_OP_CODE_WRITE:
            //TODO
            break;
        case TFS_OP_CODE_READ:
            //TODO
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            sscanf(buffer, "%d", &session_id);
            int destroyed = tfs_destroy_after_all_closed();
            strcat(result, destroyed);
            write(fcli, result, size_of(result));
            if (destroyed == 0)
                return 1;
            break;
        default:
            return -1;
    }
    return 0;
}
