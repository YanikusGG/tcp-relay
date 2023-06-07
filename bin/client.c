#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "relay_client.h"

#define BUFF_SIZE 1024

/*
 * Run ./client 127.0.0.1 8081 <type>
 * a for accept version
 * c for connect version
*/
int main(int argc, char *argv[]) {
    if (argc < 4) {
        perror("argc");
        return 1;
    }

    struct addrinfo *res = NULL;
    int gai_err;
    struct addrinfo hint = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    if ((gai_err = getaddrinfo(argv[1], argv[2], &hint, &res))) {
        printf("%s\n", gai_strerror(gai_err));
        return 1;
    }
    struct sockaddr *relay_addr = (struct sockaddr *)res->ai_addr;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Cannot create socket\n");
        freeaddrinfo(res);
        return 1;
    }
    if (argv[3][0] == 'a') {
        int id = relay_register(sock, relay_addr);
        if (id == -1) {
            fprintf(stderr, "relay_register failed\n");
            freeaddrinfo(res);
            return 1;
        }
        printf("id: %d\n", id);

        struct sockaddr *peer_addr = calloc(sizeof(*peer_addr), 1);
        if (relay_accept(sock, peer_addr)) {
            fprintf(stderr, "relay_accept error\n");
            freeaddrinfo(res);
            free(peer_addr);
            return 1;
        }

        while (1) {
            char buff[BUFF_SIZE] = {0};
            int cnt = scanf("%s", buff);
            if (cnt <= 0) {
                break;
            }

            if (write(sock, buff, cnt) != cnt) {
                fprintf(stderr, "write failed\n");
                freeaddrinfo(res);
                free(peer_addr);
                return 1;
            }
        }

        freeaddrinfo(res);
        free(peer_addr);
        return 0;
    } else if (argv[3][0] == 'c') {
        int id;
        if (scanf("%d", &id) != 1) {
            fprintf(stderr, "scanf error\n");
            freeaddrinfo(res);
            return 1;
        }

        struct sockaddr *peer_addr = calloc(sizeof(*peer_addr), 1);
        if (relay_connect(sock, id, relay_addr, peer_addr)) {
            fprintf(stderr, "relay_connect error\n");
            freeaddrinfo(res);
            free(peer_addr);
            return 1;
        }

        while (1) {
            char buff[BUFF_SIZE] = {0};
            int cnt = read(sock, buff, BUFF_SIZE);
            if (cnt <= 0) {
                fprintf(stderr, "INFO::Connect client finished reading\n");
                break;
            }
        }

        freeaddrinfo(res);
        free(peer_addr);
        return 0;
    }
    return 0;
}
