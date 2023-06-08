#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "relay_server.h"

/*
 * Run ./server 8081
 */
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

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, 0) == -1) {
        perror("sigprocmask");
        return 1;
    }
    int signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        perror("signalfd");
        return 1;
    }

    int sock_fd = create_server(argv[1]);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    ev.data.fd = sock_fd;
    int ep_ctl = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev);
    if (ep_ctl < 0) {
        perror("epoll_ctl");
        close(sock_fd);
        return 1;
    }

    listen_loop(sock_fd, epoll_fd, signal_fd);
    perror("listen_loop ends");
    close(sock_fd);
    return 1;
}
