#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include "grpc_frame.h"

#define SOCKET_PATH "/tmp/containerd_toy.sock"

static void handle_sigint(int sig) {
  (void)sig;
  unlink(SOCKET_PATH);
  printf("\n[Server] Socket unlinked and cleaned up. Exiting.\n");
  exit(0);
}

// Simple in-memory mock container state
typedef struct {
  char id[16];
  char image[64];
  bool running;
} Container;

static Container mock_containers[8];
static size_t container_count = 0;

static void handle_rpc(const char *request, char *response_out, size_t max_resp) {
  if (strncmp(request, "RunContainer ", 13) == 0) {
    const char *image = request + 13;
    if (container_count < 8) {
      snprintf(mock_containers[container_count].id, 16, "c-%zu", container_count + 1);
      strncpy(mock_containers[container_count].image, image, 63);
      mock_containers[container_count].running = true;

      snprintf(response_out, max_resp,
               "gRPC STATUS: OK (0) | Created container %s with image '%s'",
               mock_containers[container_count].id, image);
      container_count++;
    } else {
      snprintf(response_out, max_resp, "gRPC STATUS: RESOURCE_EXHAUSTED (8) | Max containers reached");
    }
  } else if (strcmp(request, "ListContainers") == 0) {
    if (container_count == 0) {
      snprintf(response_out, max_resp, "gRPC STATUS: OK (0) | No active containers");
    } else {
      int offset = snprintf(response_out, max_resp, "gRPC STATUS: OK (0) | Active Containers: [");
      for (size_t i = 0; i < container_count; i++) {
        offset += snprintf(response_out + offset, max_resp - offset,
                           "%s%s (%s: %s)",
                           i > 0 ? ", " : "",
                           mock_containers[i].id,
                           mock_containers[i].image,
                           mock_containers[i].running ? "running" : "stopped");
      }
      snprintf(response_out + offset, max_resp - offset, "]");
    }
  } else if (strncmp(request, "StopContainer ", 14) == 0) {
    const char *id = request + 14;
    bool found = false;
    for (size_t i = 0; i < container_count; i++) {
      if (strcmp(mock_containers[i].id, id) == 0) {
        mock_containers[i].running = false;
        found = true;
        break;
      }
    }
    if (found) {
      snprintf(response_out, max_resp, "gRPC STATUS: OK (0) | Container %s stopped", id);
    } else {
      snprintf(response_out, max_resp, "gRPC STATUS: NOT_FOUND (5) | Container %s not found", id);
    }
  } else {
    snprintf(response_out, max_resp, "gRPC STATUS: UNIMPLEMENTED (12) | Unknown method: '%s'", request);
  }
}

int main(void) {
  signal(SIGINT, handle_sigint);

  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket(AF_UNIX)");
    exit(EXIT_FAILURE);
  }

  unlink(SOCKET_PATH);

  struct sockaddr_un address = {0};
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  chmod(SOCKET_PATH, 0666);

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    close(server_fd);
    unlink(SOCKET_PATH);
    exit(EXIT_FAILURE);
  }

  printf("========================================================\n");
  printf(" 🐳 containerd_toy Daemon Listening on: %s\n", SOCKET_PATH);
  printf("========================================================\n");

  struct sockaddr_un client = {0};
  socklen_t length = sizeof(client);

  int client_fd = accept(server_fd, (struct sockaddr *)&client, &length);
  if (client_fd < 0) {
    perror("accept");
    close(server_fd);
    unlink(SOCKET_PATH);
    exit(EXIT_FAILURE);
  }

  // 1. Peer Credential Verification (Authentication Layer)
  uid_t client_uid = 0;
  gid_t client_gid = 0;
  getpeereid(client_fd, &client_uid, &client_gid);

  pid_t client_pid = 0;
  socklen_t pid_len = sizeof(client_pid);
  getsockopt(client_fd, SOL_LOCAL, LOCAL_PEERPID, &client_pid, &pid_len);

  printf("[Daemon] Accepted connection!\n");
  printf("[Daemon] Authenticated Peer -> PID: %d, UID: %d, GID: %d\n",
         client_pid, client_uid, client_gid);

  // 2. Process gRPC framed requests over the UDS connection
  uint8_t payload[1024];
  uint32_t payload_len = 0;

  while (grpc_recv_frame(client_fd, payload, sizeof(payload) - 1, &payload_len)) {
    payload[payload_len] = '\0';
    printf("\n[Daemon] <-- Received gRPC Frame (%u bytes): \"%s\"\n",
           payload_len, (char *)payload);

    char response[1024] = {0};
    handle_rpc((char *)payload, response, sizeof(response) - 1);

    printf("[Daemon] --> Sending gRPC Frame Response: \"%s\"\n", response);
    grpc_send_frame(client_fd, response, (uint32_t)strlen(response));
  }

  printf("\n[Daemon] Client closed stream. Cleaning up.\n");
  close(client_fd);
  close(server_fd);
  unlink(SOCKET_PATH);

  return 0;
}
