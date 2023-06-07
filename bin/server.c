#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "relay_server.h"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        perror("argc");
        return 1;
    }
    int epoll_fd = epoll_create(1);
    if (epoll_fd < 0) {
        perror("epoll_create");
        return 1;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;

    int sock_fd = create_server(argv[1]);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    ev.data.fd = sock_fd;
    int ep_ctl = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev);
    if (ep_ctl < 0) {
        perror("epoll_ctl");
        return NULL;
    }

    listen_loop(sock_fd, epoll_fd);
    return 0;
}
