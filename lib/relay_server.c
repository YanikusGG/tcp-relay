#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#include "relay_server.h"

#define MAX_ROOMS 10000

conn_t *host_conn[MAX_ROOMS];
conn_t *client_conn[MAX_ROOMS];

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

void listen_loop(int socket_fd, int epoll_fd, int signal_fd) {
    struct epoll_event events[16];
    struct sockaddr_in addr_tcp;
    struct epoll_event ev;

    struct epoll_event sig_event;
    sig_event.data.fd = signal_fd;
    sig_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &sig_event) == -1) {
        perror("epoll_ctl");
        return;
    }

    int tfds[MAX_ROOMS] = {0};

    while (1) {
        int event_cnt = epoll_wait(epoll_fd, events, 16, -1);
        if (event_cnt < 0) {
            perror("epoll_wait");
            break;
        }
        for (int ev_idx = 0; ev_idx < event_cnt; ev_idx++) {
            if (events[ev_idx].data.fd == signal_fd) {
                // signal to terminate
                printf("Start terminating\n");
                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (host_conn[i]) {
                        if (host_conn[i]->fd) {
                            close(host_conn[i]->fd);
                        }
                        free(host_conn[i]);
                    }
                    if (client_conn[i]) {
                        if (client_conn[i]->fd) {
                            close(client_conn[i]->fd);
                        }
                        free(client_conn[i]);
                    }
                }
                close(socket_fd);
                close(epoll_fd);
                close(signal_fd);
                exit(0);
            }

            if (events[ev_idx].data.fd == socket_fd) {
                // new connection
                printf("new connection\n");
                socklen_t len = sizeof(addr_tcp);
                int client_conn =
                    accept(socket_fd, (struct sockaddr *)&addr_tcp, &len);
                if (client_conn < 0) {
                    perror("accept");
                    break;
                }
                // set it as non-blocking
                int flags = fcntl(client_conn, F_GETFL, 0);
                if (flags < 0) {
                    perror("fcntl 1");
                    break;
                }
                flags |= O_NONBLOCK;
                int fc_i = fcntl(client_conn, F_SETFL, flags);
                if (fc_i < 0) {
                    perror("fcntl 2");
                    break;
                }
                conn_t *conn = malloc(sizeof(conn_t));
                conn->left = client_conn;
                conn->left_addr = addr_tcp;
                conn->right = -1;
                conn->fd = client_conn;
                conn->id = -1;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = conn;
                int ep_ctl =
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_conn, &ev);
                if (ep_ctl < 0) {
                    perror("epoll_ctl");
                    break;
                }
            } else {
                ev = events[ev_idx];
            }
            conn_t *conn = (conn_t *)(ev.data.ptr);
            printf("event fd=%d left=%d right=%d\n", conn->fd, conn->left,
                   conn->right);
            if (conn->right == -1) {
                if (tfds[conn->tfd % MAX_ROOMS] == 1) {
                    // timer triggered (true)
                    tfds[conn->tfd % MAX_ROOMS] == 0;
                    int id = conn->id;
                    printf("Timer of ID %d is over\n", id);
                    close(conn->left);
                    free(host_conn[id]);
                    host_conn[id] = NULL;
                    continue;
                }
                if (events[ev_idx].events == EPOLLIN) {
                    printf("epoll in\n");
                    char buf[1024];
                    if (recv(conn->fd, buf, 1024, 0) < 0) {
                        perror("recv 1");
                        break;
                    }
                    union {
                        int id;
                        char str[4];
                    } union_id;
                    memcpy(union_id.str, buf + 1, 4);
                    char flag = buf[0];
                    int id = union_id.id;
                    conn->id = id % MAX_ROOMS;
                    if (flag == 1) {
                        // REGISTER
                        printf("Register ID %d (%d)\n", id % MAX_ROOMS, id);
                        if (host_conn[id % MAX_ROOMS]) {
                            printf("Reject: this ID is already registered\n");
                            continue;
                        }
                        host_conn[id % MAX_ROOMS] = conn;

                        int tfd;
                        if ((tfd = timerfd_create(CLOCK_MONOTONIC,
                                                  TFD_NONBLOCK)) < 0) {
                            perror("timerfd_create");
                            break;
                        }
                        struct itimerspec newValue, oldValue;
                        struct timespec ts;
                        ts.tv_sec = 10;
                        ts.tv_nsec = 0;
                        struct timespec ts_inter;
                        ts_inter.tv_sec = 0;
                        ts_inter.tv_nsec = 0;
                        newValue.it_value = ts;
                        newValue.it_interval = ts_inter;
                        if (timerfd_settime(tfd, 0, &newValue, &oldValue) < 0) {
                            perror("timerfd_settime");
                            break;
                        }
                        tfds[tfd % MAX_ROOMS] = 1;
                        struct epoll_event tfd_event;
                        tfd_event.events = EPOLLIN | EPOLLET;
                        tfd_event.data.ptr = conn;
                        conn->tfd = tfd;
                        int ep_ctl =
                            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tfd, &tfd_event);
                        if (ep_ctl < 0) {
                            perror("epoll_ctl");
                            break;
                        }
                    } else {
                        // CONNECT
                        printf("Connect ID %d (%d)\n", id % MAX_ROOMS, id);
                        if (!host_conn[id % MAX_ROOMS]) {
                            printf("Reject: this ID wasn't registered\n");
                            close(conn->fd);
                            free(conn);
                            continue;
                        }
                        if (host_conn[id % MAX_ROOMS]->right != -1) {
                            printf("Reject: this ID is used in another "
                                   "connection\n");
                            close(conn->fd);
                            free(conn);
                            continue;
                        }
                        client_conn[id % MAX_ROOMS] = conn;
                        host_conn[id % MAX_ROOMS]->right = conn->left;
                        client_conn[id % MAX_ROOMS]->right =
                            host_conn[id % MAX_ROOMS]->left;
                        host_conn[id % MAX_ROOMS]->right_addr = conn->left_addr;
                        client_conn[id % MAX_ROOMS]->right_addr =
                            host_conn[id % MAX_ROOMS]->left_addr;

                        int ls = send(
                            host_conn[id % MAX_ROOMS]->left,
                            &host_conn[id % MAX_ROOMS]->right_addr,
                            sizeof(host_conn[id % MAX_ROOMS]->right_addr), 0);
                        if (ls < 0) {
                            perror("send 1");
                            return;
                        }

                        int rs = send(
                            host_conn[id % MAX_ROOMS]->right,
                            &host_conn[id % MAX_ROOMS]->left_addr,
                            sizeof(host_conn[id % MAX_ROOMS]->left_addr), 0);
                        if (rs < 0) {
                            perror("send 2");
                            return;
                        }
                    }
                }
            } else {
                if (tfds[conn->tfd % MAX_ROOMS] == 1) {
                    // timer triggered (false)
                    printf("Non-actual timer triggered\n");
                    tfds[conn->tfd % MAX_ROOMS] == 0;
                    continue;
                }
                // RECEIVE FROM KNOWN ROOM
                printf("Known room\n");
                char buf[1024] = {0};
                int recv_size = recv(conn->left, buf, 1024, 0);
                if (recv_size <= 0) {
                    printf("Closing connection between %d and %d\n", conn->left,
                           conn->right);
                    int id = conn->id;
                    close(conn->left);
                    close(conn->right);
                    free(host_conn[id]);
                    free(client_conn[id]);
                    host_conn[id] = NULL;
                    client_conn[id] = NULL;
                    break;
                }
                printf("Message: %s\n", buf);
                int rs = send(conn->right, buf, recv_size, 0);
                if (rs < 0) {
                    perror("send 3");
                    return;
                }
                printf("Sent from %d to %d\n", conn->left, conn->right);
            }
        }
    }
}
