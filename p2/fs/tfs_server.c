#include "operations.h"
#include <fcntl.h>

int main(int argc, char **argv) {
    //
    char* session[MAX_SESSIONS];
    char buffer[MAX_REQUEST_SIZE];
    char results[MAX_REQUEST_SIZE];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    int fserv;
    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0)
        exit(1);
    if ((fserv = open(pipename, O_RDONLY)) < 0)
        exit(1);
    
    
    for (;;) {
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

// int open_session(char const* client_pipe_path)
    // se houver espaço para sessões
        // session[session_id] aponta para o client_pipe_path
        // liga os pipes ????
        // return do session_id
    // se não houver
        // return -1

// int close_session(session_id)
    // session[session_id] manda para char* client_pipe
    // session[session_id] = NULL
    // close desse client_pipe

//char* handle_request() {
//    switch(OP_code):
//    case
//    return results;
//}