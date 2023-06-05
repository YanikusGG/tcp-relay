#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "relay_server.h"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        perror("argc");
        return 1;
    }
    int sock_fd = create_server(argv[1]);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }
    listen_loop(sock_fd);
    return 0;
}
