#include "operations.h"
#include <fcntl.h>

int main(int argc, char **argv) {

    char* session[MAX_SESSIONS];

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
    
    // TODO
    /*for (;;) {
        n = read (fserv, buf, TAMMSG);
        if (n <= 0) break;
        trataPedido (buf);
        n = write (fcli, buf, TAMMSG);
    }
    */
    close (fserv);
    unlink(pipename);
    return 0;
}