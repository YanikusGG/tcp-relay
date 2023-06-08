#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct {
    int fd;
    int left, right;
    int id;
    struct sockaddr_in left_addr, right_addr;
} conn_t;

typedef enum { CLIENT, SERVER } conn_type_t;

int create_server(char *port);
void listen_loop(int socket_fd, int epoll_fd);
