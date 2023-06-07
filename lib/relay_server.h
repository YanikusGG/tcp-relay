typedef struct {
    int fd;
    int left, right;
} conn_t;

typedef enum { CLIENT, SERVER } conn_type_t;

int create_server(char *port);
void listen_loop(int socket_fd, int epoll_fd);
