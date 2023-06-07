#include "relay_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>

#define CONNECT_FLAG 0
#define REGISTER_FLAG 1
#define REGISTER_MESSAGE_LEN 5

int relay_register(int fd, const struct sockaddr *relay_addr) {
    srand(time(NULL));

    char buff[REGISTER_MESSAGE_LEN];
    buff[0] = REGISTER_FLAG;
    int *id = (int *)(buff + 1);
    *id = rand();

    if (connect(fd, relay_addr, sizeof(*relay_addr)) < 0) {
        fprintf(stderr, "Connect returned error\n");
        return -1;
    }
    if (write(fd, buff, REGISTER_MESSAGE_LEN) != 5) {
        fprintf(stderr, "Write failed\n");
        return -1;
    }
    return *id;
}

int relay_accept(int fd, struct sockaddr *peer_addr) {
    if (read(fd, peer_addr, sizeof(*peer_addr)) != sizeof(*peer_addr)) {
        fprintf(stderr, "Read failed\n");
        return 1;
    }
    return 0;
}

int relay_connect(int fd, int connection_id, const struct sockaddr *relay_addr, struct sockaddr *peer_addr) {
    char buff[REGISTER_MESSAGE_LEN];
    buff[0] = CONNECT_FLAG;
    int *id = (int *)(buff + 1);
    *id = connection_id;

    if (connect(fd, relay_addr, sizeof(*relay_addr)) < 0) {
        fprintf(stderr, "Connect returned error\n");
        return 1;
    }
    if (write(fd, buff, REGISTER_MESSAGE_LEN) != 5) {
        fprintf(stderr, "Write failed\n");
        return 1;
    }

    if (read(fd, peer_addr, sizeof(*peer_addr)) != sizeof(*peer_addr)) {
        fprintf(stderr, "Read failed\n");
        return 1;
    }
    return 0;
}
