#include <sys/socket.h>


int relay_register(int fd, const struct sockaddr *relay_addr);
int relay_accept(int fd, struct sockaddr *peer_addr);
int relay_connect(int fd, const struct sockaddr *relay_addr, struct sockaddr *peer_addr);
