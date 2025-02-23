#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests. printf("Logs from your program will appear here!\n");
  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  // Creates a TCP socket (SOCK_STREAM) using IPv4 addressing (AF_INET) with
  // default protocol (0)
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  // holds the settings of a server address, uses IPv4 (AF_INET),
  // converts the port 4221 to network byte order (standard way in data is
  // formatted when sent through a network)
  // binds to all available network interfaces htonl(INADDR_ANY)
  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  // Assigns address to the socket
  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  // maximum number of connections listened
  int connection_backlog = 5;
  // Starts listenning for connections
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
  if (client_fd < 0) {
    printf("Accept failed: %s\n", strerror(errno));
    return 1;
  }
  printf("Client connected\n");

  const char *http_response = "HTTP/1.1 200 OK\r\n\r\n";
  send(client_fd, http_response, strlen(http_response), 0);
  
  close(client_fd);
  close(server_fd);

  return 0;
}
