#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    char *directory = NULL;

    for (int i = 1; i < argc - 1; i++) {
        if (argc > 1 && strcmp(argv[i], "--directory") == 0) {
            directory = argv[i + 1];
            break;
        }
    }
    if (!directory) {
        fprintf(stderr, "--directory flag is required\n");
        return 1;
    }

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

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                               &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd); // child doesn't need the listening socket

            char buffer[BUFFER_SIZE] = {0};
            ssize_t bytes_received =
                recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                close(client_fd);
                exit(1);
            }

            buffer[bytes_received] = '\0';

            char method[BUFFER_SIZE], path[BUFFER_SIZE], protocol[BUFFER_SIZE];
            sscanf(buffer, "%s %s %s", method, path, protocol);
            path[BUFFER_SIZE - 1] = '\0';

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
            } else if (strcmp(path, "/user-agent") == 0) {
                char user_agent[BUFFER_SIZE] = {0};
                char *user_agent_line = strstr(buffer, "User-Agent: ");
                if (user_agent_line) {
                    user_agent_line += 12;
                    char *end_of_line = strstr(user_agent_line, "\r\n");
                    if (end_of_line) {
                        size_t user_agent_len = end_of_line - user_agent_line;
                        strncpy(user_agent, user_agent_line, user_agent_len);
                        user_agent[user_agent_len] = '\0';
                    }
                }

                int content_length = strlen(user_agent);
                snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: %d\r\n"
                         "\r\n"
                         "%s",
                         content_length, user_agent);
                send(client_fd, response, strlen(response), 0);
            } else if (strncmp(path, "/files/", 7) == 0) {
                const char *filename = path + 7;
                char filepath[BUFFER_SIZE];
                // sets the filepath to the directory and the filename
                snprintf(filepath, sizeof(filepath), "%s/%s", directory,
                filename);

                if (strcmp(method, "POST") == 0) {
                    // Get the length of the content in the request
                    char *cl_start = strstr(buffer, "Content-Length: ");
                    int content_length = 0;
                    if (cl_start) {
                        sscanf(cl_start, "Content-Length: %d", &content_length);
                    }

                    // Find the start of the body
                    char *body = strstr(buffer, "\r\n\r\n");
                    if (body) {
                        body += 4; // skip past "\r\n\r\n"
                    }

                    int bytes_in_buffer = bytes_received - (body - buffer);
                    char *request_body = malloc(content_length);
                    memcpy(request_body, body, bytes_in_buffer);

                    int bytes_remaining = content_length - bytes_in_buffer;
                    int offset = bytes_in_buffer;

                    // read the rest of the body
                    while (bytes_remaining > 0) {
                        ssize_t n = recv(client_fd, request_body + offset,
                                         bytes_remaining, 0);
                        if (n <= 0)
                            break;
                        offset += n;
                        bytes_remaining -= n;
                    }

                    FILE *f = fopen(filepath, "wb");
                    if (f) {
                        fwrite(request_body, 1, content_length, f);
                        fclose(f);

                        const char *created_response =
                            "HTTP/1.1 201 Created\r\n\r\n";
                        send(client_fd, created_response,
                             strlen(created_response), 0);
                    } else {
                        const char *server_error =
                            "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                        send(client_fd, server_error, strlen(server_error), 0);
                    }

                    free(request_body);
                } else if (strcmp(method, "GET") == 0) {
                    FILE *file = fopen(filepath, "rb");
                    if (!file) {
                        const char *not_found =
                            "HTTP/1.1 404 Not Found\r\n\r\n";
                        send(client_fd, not_found, strlen(not_found), 0);
                    } else {
                        fseek(file, 0, SEEK_END);
                        long filesize = ftell(file);
                        rewind(file);

                        char *file_content = malloc(filesize);
                        fread(file_content, 1, filesize, file);
                        fclose(file);

                        snprintf(response, sizeof(response),
                                 "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/octet-stream\r\n"
                                 "Content-Length: %ld\r\n"
                                 "\r\n",
                                 filesize);
                        send(client_fd, response, strlen(response), 0);
                        send(client_fd, file_content, filesize, 0);
                        free(file_content);
                    }
                }
            } else {
                const char *not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                                 "Content-Type: text/plain\r\n"
                                                 "Content-Length: 14\r\n"
                                                 "\r\n"
                                                 "Page not found";
                send(client_fd, not_found_response, strlen(not_found_response),
                     0);
            }

            shutdown(client_fd, SHUT_WR);
            close(client_fd);
            exit(0); // Important to exit child process
        } else {
            // Parent process
            close(client_fd); // parent closes client socket; child handles it
        }
    }

    close(server_fd);

    return 0;
}