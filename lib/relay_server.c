#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdlib.h>

#include "relay_server.h"

conn_t host_conn[1024];
conn_t client_conn[1024];

int create_server(char *port) {
    struct addrinfo *res = NULL;
    struct addrinfo hint = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
    };
    int gai_err = getaddrinfo(NULL, port, &hint, &res);
    if (gai_err) {
        return -1;
    }
    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            sock = -1;
            continue;
        }
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (listen(sock, SOMAXCONN) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(res);
    return sock;
}

void listen_loop(int socket_fd, int epoll_fd) {
    struct epoll_event events[16];
    struct sockaddr_in addr_tcp;
    struct epoll_event ev;

    while (1) {
        int event_cnt = epoll_wait(epoll_fd, events, 16, -1);
        if (event_cnt < 0) {
            perror("epoll_wait");
            break;
        }
        for (int ev_idx = 0; ev_idx < event_cnt; ev_idx++) {
            if (events[ev_idx].data.fd == socket_fd) {
                // new connection.
                socklen_t len = sizeof(addr_tcp);
                int client_conn = accept(socket_fd, (struct sockaddr *)&addr_tcp, &len);
                if (client_conn < 0) {
                    perror("accept");
                    break;
                }
                // set it as non-blocking
                int flags = fcntl(client_conn, F_GETFL, 0);
                if (flags < 0) {
                    perror("fcntl");
                    break;
                }
                flags |= O_NONBLOCK;
                int fc_i = fcntl(client_conn, F_SETFL, flags);
                if (fc_i < 0) {
                    perror("fcntl");
                    break;
                }
                conn_t *conn = malloc(sizeof(conn_t));
                conn->left = client_conn;
                conn->fd = client_conn;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = conn;
                int ep_ctl = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_conn, &ev);
                if (ep_ctl < 0) {
                    perror("epoll_ctl");
                    break;
                }

            }
            conn_t *conn = (conn_t*)(events[ev_idx].data.ptr);
            printf("%d %d %d\n", conn->fd, conn->left, conn->right);
            if (events[ev_idx].events == EPOLLIN) {
                printf("epoll in\n");
                char buf[1024];
                int ret_i = recv(conn->fd, buf, sizeof(buf), 0);
                if (ret_i < 0) {
                    perror("recv");
                    break;
                }
                printf("ready to send");
            }
        }
    }
}
