#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int server_fd;
    socklen_t client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
        0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(4221),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) !=
        0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        printf("Accept failed: %s\n", strerror(errno));
        return 1;
    }
    printf("Client connected\n");

    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Failed to receive request\n");
        close(client_fd);
    }

    buffer[bytes_received] = '\0';

    char method[BUFFER_SIZE], path[BUFFER_SIZE], protocol[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, path, protocol);
    path[BUFFER_SIZE - 1] = '\0';
    printf("Request path: %s\n", path);

    char response[BUFFER_SIZE];
    if (strcmp(path, "/") == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 2\r\n"
                 "\r\n"
                 "OK");
        send(client_fd, response, strlen(response), 0);
    } else if (strncmp(path, "/echo/", 6) == 0) {
        const char *echo_str = path + 6;
        int content_length = strlen(echo_str);
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %d\r\n"
                 "\r\n"
                 "%s",
                 content_length, echo_str);
        send(client_fd, response, strlen(response), 0);
    } else {
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 15\r\n"
                               "\r\n"
                               "Page not found\n";
        send(client_fd, response, strlen(response), 0);
    }

    shutdown(client_fd, SHUT_WR);
    close(server_fd);

    return 0;
}