#include "operations.h"
#include <fcntl.h>

char* session[MAX_SESSIONS];
char buffer[MAX_REQUEST_SIZE];
char results[MAX_REQUEST_SIZE];
int fserv, fcli;

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
        int n = read(fserv, buffer, sizeof(buffer));
        if (n <= 0) break;
        results = handle_request(buffer);
            //buffer = "OPCODE:session_id:arguments"
        n = write (fcli, results, size_of(results));
    }
    close (fserv);
    unlink(pipename);
    return 0;
}

int open_session(char const* client_pipe_path) {

    int session_id = try_session();
    if (session_id != -1)
        session[session_id] = client_pipe_path;
        if (fcli = open(client_pipe_path, O_WRONLY) < 0) return -1;
        return session_id;
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
    close(fcli);
    unlink(client_pipe_path);
    return 0;
}

char* handle_request(char* buffer) {
    int op_code, session_id;
    char* client_pipe_path;
    sscanf(buffer, "%d", &op_code);
    buffer++;
    switch (op_code) {
        case TFS_OP_CODE_MOUNT:
            sscanf(buffer, "%s", client_pipe_path);
            session_id = open_session(client_pipe_path);
            if (session_id != -1)
                if (write(fcli, &session_id, sizeof(int))) return -1;
            break;
        case TFS_OP_CODE_UNMOUNT:
            sscanf(buffer, "%d", &session_id);
            close_session(session_id);
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
            //TODO
            break;
        default:
            return -1;
    }
    return -1;
}
