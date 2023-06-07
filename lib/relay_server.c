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
#define MAX_ROOMS 65535

conn_t* host_conn[MAX_ROOMS];
conn_t* client_conn[MAX_ROOMS];

int create_server(char *port) {
    memset(host_conn, sizeof(host_conn), 0);
    memset(client_conn, sizeof(client_conn), 0);
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
                conn->left_addr = addr_tcp;
                conn->right = -1;
                conn->fd = client_conn;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = conn;
                int ep_ctl = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_conn, &ev);
                if (ep_ctl < 0) {
                    perror("epoll_ctl");
                    break;
                }

            }
            conn_t *conn = (conn_t*)(ev.data.ptr);
            printf("%d %d %d\n", conn->fd, conn->left, conn->right);
            if (conn->right == -1) {
                if (events[ev_idx].events == EPOLLIN) {
                    printf("epoll in\n");
                    char buf[1024];
                    int ret_i = recv(conn->fd, buf, sizeof(buf), 0);
                    if (ret_i < 0) {
                        perror("recv");
                        break;
                    }
                    char flag = buf[0];
                    int id = *(int*)(buf + 1);
                    if (flag == 1) {
                        // REGISTER
                        printf("Register %d \n", id % MAX_ROOMS);
                        host_conn[id % MAX_ROOMS] = conn;
                    } else {
                        // CONNECT
                        printf("Connect %d \n", id % MAX_ROOMS);
                        client_conn[id % MAX_ROOMS] = conn;
                        host_conn[id % MAX_ROOMS]->right = conn->left;
                        client_conn[id % MAX_ROOMS]->right = host_conn[id % MAX_ROOMS]->left;
                        host_conn[id % MAX_ROOMS]->right_addr = conn->left_addr;
                        client_conn[id % MAX_ROOMS]->right_addr = host_conn[id % MAX_ROOMS]->left_addr;

                        int ls = send(host_conn[id % MAX_ROOMS]->left, &host_conn[id % MAX_ROOMS]->right_addr, sizeof(host_conn[id % MAX_ROOMS]->right_addr), 0);
                        if (ls < 0) {
                            perror("send");
                            return 1;
                        }

                        int rs = send(host_conn[id % MAX_ROOMS]->right, &host_conn[id % MAX_ROOMS]->left_addr, sizeof(host_conn[id % MAX_ROOMS]->left_addr), 0);
                        if (rs < 0) {
                            perror("send");
                            return 1;
                        }
                    }
                }
            } else {
                // RECEIVE FROM KNOWN ROOM
            }
        }
    }
}
